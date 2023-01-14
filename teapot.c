
#include "teapot.h"

// Try and guess what just tried to call us and respond nicely.

void
teapot(uint8_t * buf, int len)
{
    int dump_it = 1;
    int rv = 0;
    int send_logout = 0;
    int send_tls_fail = 0;
    int send_http_error = 0;

    int is_ascii = 1;
    for(int i = 0; is_ascii && i<len; i++)
	if (!(buf[i] == '\r' || buf[i] == '\n' || (buf[i] >= ' ' && buf[i] <= '~')))
	    is_ascii = 0;

    if (client_ipv4_port) {
	int fm = 1;
	if (len == 0)
	    rv = dump_it = 0;

	else if (len > 0 && len < 5) {
	    char hbuf[32] = "";
	    for(int i=0; i<len; i++)
		sprintf(hbuf+i*6, ", 0x%02x", buf[i]);
	    printlog("Failed connect from %s, byte%s %s",
		client_ipv4_str, len>1?"s":"", hbuf+2);
	    fm = rv = dump_it = 0;

	} else if (buf[0] == 0x16 && buf[1] == 0x03 && buf[1] >= 1 && buf[1] <= 3) { 
	    printlog("Failed connect from %s, a TLS Client hello packet",
		client_ipv4_str);
	    fm = rv = dump_it = 0;
	    send_tls_fail = 1;

	} else if (len > 32 && buf[0] == 3 && !buf[1] && !buf[2] && buf[3] == len &&
		buf[4] == len-5 && memcmp(buf+11, "Cookie: mstshash=", 17) == 0) {
	    uint8_t * p = buf+28;
	    char nbuf[256] = "";
	    int i = 0;
	    while(p<buf+len && *p > ' ' && *p < '~') {
		nbuf[i++] = *p++; nbuf[i] = 0;
	    }
	    printlog("Failed connect from %s, RDP for \"%s\"", client_ipv4_str, nbuf);
	    fm = rv = dump_it = 0;

	} else if (len >= 19 && buf[0] == 3 && !buf[1] && !buf[2] && buf[3] == 19 &&
		buf[4] == 14 && buf[5] == 0xe0 && !buf[6] && !buf[7]) {
	    printlog("Failed connect from %s, RDP client hello", client_ipv4_str);
	    fm = rv = dump_it = 0;

	} else if (len > 12 && buf[1] == 0 && buf[0] >= 12 && buf[0] < len && buf[0] < 32 &&
	    buf[buf[0]-2] == ((tcp_port_no>>8)&0xFF) && buf[buf[0]-1] == (tcp_port_no&0xFF)) {
	    printlog("Failed connect from %s, %s", client_ipv4_str,
		"Using a Minecraft protocol.");
	    fm = rv = dump_it = 0;
	    send_logout = 1;

	} else if (len > 12 && buf[0] < 4 && buf[3] > 1 && buf[3]*2+5 < len && buf[3] <= 16 &&
	    buf[buf[3]*2+4] == 0 && buf[buf[3]*2+5] < 16 &&
	    buf[buf[3]*2+5]*2+buf[3]*2+9 < len &&
	    buf[buf[buf[3]*2+5]*2+buf[3]*2+8]  == ((tcp_port_no>>8)&0xFF) &&
	    buf[buf[buf[3]*2+5]*2+buf[3]*2+9]  == (tcp_port_no&0xFF) ) {
	    printlog("Failed connect from %s, %s", client_ipv4_str,
		"Using slightly newer Minecraft protocol.");
	    fm = rv = dump_it = 0;
	    send_logout = 1;

	} else if (len > 14 &&
		(memcmp(buf, "GET ", 4) == 0 || memcmp(buf, "HEAD ", 5) == 0)) {
	    send_http_error = 1;
	    fm = rv = 0;

	} else if (len > 14 && memcmp(buf, "PRI * HTTP/2.0", 14) == 0) {
	    send_http_error = 1;
	    printlog("Failed connect from %s, %s", client_ipv4_str,
		"HTTP/2.0 client hello.");
	    fm = rv = dump_it = 0;

	}
	if (fm)
	    printlog("Failed connect from %s, %s", client_ipv4_str,
		len ? "invalid client hello": "no data.");
    }
    else if (len <= 0)
	printlog("Failed connect from unknown remote, nothing received.");

    if (dump_it) {
	if (is_ascii) {
	    char message[65536];
	    int j = 0;
	    for(int i = 0; i<len && j<(int)sizeof(message)-4; i++) {
		if (buf[i] >= ' ') message[j++] = buf[i];
		else {
		    message[j++] = '\\';
		    if (buf[i] == '\n') message[j++] = 'n';
		    if (buf[i] == '\r') message[j++] = 'r';
		}
	    }
	    message[j] = 0;
	    printlog("Text received was: %s", message);
	} else {
	    for(int i = 0; i<len; i++)
		hex_logfile(buf[i]);
	    hex_logfile(EOF);
	}
    }

    if (line_ofd > 0)
    {
	if (send_http_error) {
	    unsigned char msg[] =
		"HTTP/1.1 426 Upgrade Required\r\n"
		"Upgrade: mc/0.30+CPE, websocket, mc/0.30\r\n"
		"\r\n";
	    write_to_remote(msg, sizeof(msg)-1);
	} else

	if (send_logout) {
	    unsigned char buf[65];
	    char msg[] = "This is a classic (0.30) minecraft server with CPE";
	    memset(buf, ' ', sizeof(buf));
	    memcpy(buf+1, msg, sizeof(msg)-1);
	    buf[0] = PKID_DISCON;
	    write_to_remote(buf, sizeof(buf));
	} else

#if 0
	if (send_new_logout) {
	    // This should be a new protocol disconnect message.
	    // Don't send this there are stupid scanners out there.
	    unsigned char msg[] =  {
		     70,   0,  68, '{', '"', 't', 'e', 'x', 't', '"',
		    ':', ' ', '"', 'O', 'n', 'l', 'y', ' ', 'c', 'l',
		    'a', 's', 's', 'i', 'c', ' ', 'p', 'r', 'o', 't',
		    'o', 'c', 'o', 'l', ' ', 's', 'e', 'v', 'e', 'n',
		    ' ', 'i', 's', ' ', 's', 'u', 'p', 'p', 'o', 'r',
		    't', 'e', 'd', '"', ',', ' ', '"', 'c', 'o', 'l',
		    'o', 'r', '"', ':', ' ', '"', 'r', 'e', 'd', '"',
		    '}', 1, 0, 0 };
	    write_to_remote(msg, sizeof(msg)-1);
	} else
#endif

	if (send_tls_fail) {
	    unsigned char msg[] = { 0x15, 0x03, 0x01, 0x00, 0x02, 0x02, 80 };
	    write_to_remote(msg, sizeof(msg));
	} else

	{
	    unsigned char msg[] = "418 This is a classic (0.30) minecraft server with CPE\r\n";
	    write_to_remote(msg, sizeof(msg)-1);
	}

	flush_to_remote();
	shutdown(line_ofd, SHUT_RDWR);
	if (line_ofd != line_ifd)
	    shutdown(line_ifd, SHUT_RDWR);
    }
    exit(rv);
}
