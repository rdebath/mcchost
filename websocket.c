
#include "websocket.h"

/* These are shim functions to insert into the normal socket processing to
   allow a websocket connection to be accepted instead.
 */

#define OPCODE_NOP	0	// Continued
#define OPCODE_TEXT	1
#define OPCODE_BINARY	2
#define OPCODE_DISCON	8
#define OPCODE_PING	9
#define OPCODE_PONG	10
/* 3..7,11..15 Reserved */

/* TODO: Fragments, PING/PONG */

int websocket = 0;
static char mask_value[4] = {0};
static int mask_id = 0;
static uint64_t packet_len = 0;
static char websocket_buffer[32];
static int websocket_buffer_sz = 0;

int
websocket_startup(uint8_t * buf, int buflen)
{
    enum {
	has_host=1, has_upgrade=2, has_connect=4, has_key=8, has_proto=16,
	has_ver=32, has_origin=64
    } has_stuff = 0;

    uint8_t *key_ptr = 0;
    int key_len = 0;

    // Basic check that the HTTP request looks websocketish.
    if (memcmp(buf, "GET ", 4) != 0) return 0;
    for(uint8_t *p = buf; *p; p++) {
	if (p-buf >= buflen) return 0;
	if (*p != '\n') continue;

	if (strncasecmp(p+1, "Host", 4) == 0) has_stuff |= has_host;
	if (strncasecmp(p+1, "Origin", 6) == 0) has_stuff |= has_origin;
	if (strncasecmp(p+1, "Upgrade", 7) == 0) {
	    uint8_t *p2 = p+8;
	    while (*p2 == ' ' || *p2 == ':') p2++;
	    if (strncasecmp(p2, "websocket", 9) == 0 && (p2[9] <= ' ' || p2[9] == ','))
		has_stuff |= has_upgrade;
	}
	if (strncasecmp(p+1, "Connection", 10) == 0) {
	    uint8_t *p2 = p+12;
	    while (*p2 == ' ' || *p2 == ':') p2++;
	    if (strncasecmp(p2, "upgrade", 7) == 0 && p2[7] <= ' ')
		has_stuff |= has_connect;
	}
	if (strncasecmp(p+1, "Sec-WebSocket-Key", 17) == 0) {
	    key_ptr = p+19;
	    while (*key_ptr == ' ' || *key_ptr == ':') key_ptr++;
	    uint8_t *p2 = key_ptr;
	    while (*p2 > ' ') p2++;
	    key_len = p2-key_ptr;
	    has_stuff |= has_key;
	}
	if (strncasecmp(p+1, "Sec-WebSocket-Version", 21) == 0) {
	    uint8_t *p2 = p+23;
	    while (*p2 == ' ' || *p2 == ':') p2++;
	    if (strncasecmp(p2, "13", 2) == 0 && p2[2] <= ' ')
		has_stuff |= has_ver;
	}
	if (strncasecmp(p+1, "Sec-WebSocket-Protocol", 22) == 0)
	    has_stuff |= has_proto;
    }
    // Does it have everything?
    if (has_stuff != 0x7f) return 0;

    printlog("Sending websocket response to %s", client_ipv4_str);

    char obuf[2048], key_resp[64], sha1_buf[64];

    int l = snprintf(obuf, sizeof(obuf), "%.*s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key_len, key_ptr);
    sha1digest(sha1_buf, 0, obuf, l);
    base64(key_resp, sha1_buf, 20);

    saprintf(obuf, "%s\r\n%s\r\n%s\r\n%s%s\r\n%s\r\n\r\n",
	"HTTP/1.1 101 Switching Protocols",
	"Upgrade: websocket",
	"Connection: Upgrade",
	"Sec-WebSocket-Accept: ", key_resp,
	"Sec-WebSocket-Protocol: ClassiCube");

    write_to_remote(obuf, strlen(obuf));
    flush_to_remote();
    return 1;
}

int
websocket_translate(char * inbuf, int * insize)
{
    int header_size = 2;
    char * outbuf = inbuf;
    int outsize = 0;

    while(*insize > 0)
    {
	switch(websocket)
	{
	case 1: // websocket on, nothing pending.
	    {
		header_size = 2;
		if (*insize >= header_size && ((inbuf[1]&0x80) != 0))
		    header_size += 4;
		if (*insize >= header_size && ((inbuf[1]&0x7F) == 126))
		    header_size += 2;
		if (*insize >= header_size && ((inbuf[1]&0x7F) == 125))
		    header_size += 8;
		
		if (*insize < header_size)
		{
		    // Header is too small -- save it.
		    memcpy(websocket_buffer, inbuf, *insize);
		    websocket_buffer_sz = *insize;
		    *insize = outsize;
		    websocket = 2;
		    return 0;
		}

		memcpy(websocket_buffer, inbuf, header_size);
		inbuf += header_size;
		*insize -= header_size;
		websocket = 2;
	    }
	    break;

	case 2: // websocket on, incomplete header.
	    {
		int l = *insize;
		if (websocket_buffer_sz + l > sizeof(websocket_buffer))
		    l = sizeof(websocket_buffer)-websocket_buffer_sz;
		memcpy(websocket_buffer + websocket_buffer_sz, inbuf, l);
		// websocket_buffer is valid up to (l)

		header_size = 2;
		if (l >= header_size && ((websocket_buffer[1]&0x80) != 0))
		    header_size += 4;
		if (l >= header_size && ((websocket_buffer[1]&0x7F) == 126))
		    header_size += 2;
		if (l >= header_size && ((websocket_buffer[1]&0x7F) == 125))
		    header_size += 8;

		if (l < header_size)
		{
		    // Still too small!
		    memcpy(websocket_buffer+websocket_buffer_sz, inbuf, *insize);
		    websocket_buffer_sz += *insize;
		    *insize = outsize;
		    websocket = 2;
		    return 0;
		}

		inbuf += header_size - websocket_buffer_sz;
		*insize -= header_size - websocket_buffer_sz;
		websocket = 2;
	    }
	    break;

	case 3: break; // In packet data.

	default: return 0; // Um, NOPE.
	}

	if (websocket == 2) {
	    websocket = 3;
	    memset(mask_value, 0, sizeof(mask_value));
	    mask_id = -1;
	    if ((websocket_buffer[1]&0x80) != 0) {
		memcpy(mask_value, websocket_buffer+header_size-4, 4);
		mask_id = 0;
	    }

	    packet_len = (websocket_buffer[1]&0x7F);
	    if (packet_len == 127) {
		// We really don't care how big the packet is.
		packet_len = 0;
		for(int i=0; i<8; i++)
		    packet_len = (packet_len<<8) + (uint8_t)websocket_buffer[2+i];
	    } else if (packet_len == 126) {
		packet_len =
		    (((uint8_t)websocket_buffer[2] << 8) +
		      (uint8_t)websocket_buffer[3]);
	    }
	    int op = (websocket_buffer[0] & 0xF);

	    if (op == OPCODE_DISCON) {
		// If we get a disconnect the other end will wait for us to
		// reply before closing the TCP connection. ClassiCube isn't
		// going to do anything with this reply so we just close the
		// socket instead.
		if (packet_len > 0) {
		    int ws_code = 0x1005;
		    if (packet_len >= 2) {
			ws_code = (((uint8_t)inbuf[0] << 8) +
				    (uint8_t)inbuf[1]);
		    }
		    printlog("Websocket disconnect received. (%d)", ws_code);
		    if (packet_len != 2) {
			for(int i = 0; i<packet_len && i<*insize; i++) {
			    hex_logfile((inbuf[i] ^ mask_value[mask_id]));
			    mask_id = ((mask_id+1)&3);
			}
			hex_logfile(EOF);
		    }
		} else
		    printlog("Websocket disconnect received.");
		return -1;
	    }

	    // Ignore unexpected zero length packets; hopefully that's the right thing to do.
	    if (op != OPCODE_BINARY && op != OPCODE_TEXT && packet_len != 0) {
		printlog("Websocket error opcode %d, len = %jd", op, (intmax_t)packet_len);
		for(int i = 0; i<header_size; i++)
		    hex_logfile(websocket_buffer[i]);
		for(int i = 0; i<*insize; i++) {
		    hex_logfile((inbuf[i] ^ mask_value[mask_id]));
		    mask_id = ((mask_id+1)&3);
		}
		hex_logfile(EOF);
		char ebuf[256];
		sprintf(ebuf, "Unexpected websocket opcode %d", op);
		fatal(ebuf);
	    }
	}

	int bytes_to_do = *insize;
	if (bytes_to_do > packet_len)
	    bytes_to_do = packet_len;

	if (mask_id == -1) {
	    if (outbuf+outsize != inbuf)
		for(int i = 0; i<bytes_to_do; i++) outbuf[outsize++] = inbuf[i];
	    else
		outsize += bytes_to_do;
	} else {
	    for(int i = 0; i<bytes_to_do; i++) {
		outbuf[outsize++] = (inbuf[i] ^ mask_value[mask_id]);
		mask_id = ((mask_id+1)&3);
	    }
	}
	*insize -= bytes_to_do;
	inbuf += bytes_to_do;
	packet_len -= bytes_to_do;
	if (packet_len <= 0)
	    websocket = 1; // Packet header.
    }

    *insize = outsize;
    return 0;
}

void
websocket_header(char * buf, int * len, uint32_t buflen)
{
    *len = 2;
    buf[0] = OPCODE_BINARY;
    buf[0] |= 0x80; // No fragments
    buf[1] = 0; // No Mask
    if (buflen < 126) {
	buf[1] |= buflen;
    } else if (buflen < 65536) {
	*len += 2;
	buf[1] |= 126;
	buf[2] = ((buflen >> 8) & 0xFF);
	buf[3] = (buflen & 0xFF);
    } else {
	*len += 8;
	buf[1] |= 127;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0;
	buf[5] = 0;
	buf[6] = ((buflen >> 24) & 0xFF);
	buf[7] = ((buflen >> 16) & 0xFF);
	buf[8] = ((buflen >> 8) & 0xFF);
	buf[9] = (buflen & 0xFF);
    }
}

void
base64(uint8_t * out, const uint8_t *in, int inlen)
{
    static char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int s = 0, i, j, ch, pc = 0;

    for (i = j = 0; i < inlen; i++) {
	ch = in[i];

	switch (s) {
	case 0:
	    s++;
	    out[j++] = b64[(ch >> 2) & 0x3F];
	    break;
	case 1:
	    s++;
	    out[j++] = b64[((pc & 0x3) << 4) | ((ch >> 4) & 0xF)];
	    break;
	case 2:
	    s = 0;
	    out[j++] = b64[((pc & 0xF) << 2) | ((ch >> 6) & 0x3)];
	    out[j++] = b64[ch & 0x3F];
	    break;
	}
	pc = ch;
    }

    switch (s) {
    case 1:
	out[j++] = b64[(pc & 0x3) << 4];
	out[j++] = '=';
	out[j++] = '=';
	break;
    case 2:
	out[j++] = b64[(pc & 0xF) << 2];
	out[j++] = '=';
	break;
    }
    out[j] = 0;
}
