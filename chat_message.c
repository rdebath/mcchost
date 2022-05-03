#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "chat_message.h"

static char * pending_chat = 0;
static int pending_chat_size = 0;
static int pending_chat_len = 0;

void
process_chat_message(int msg_flag, char * msg)
{
    // Message concat.
    if ((msg_flag == 1 && extn_longermessages) || pending_chat_len > 0) {
	if (pending_chat == 0) {
	    pending_chat = malloc(PKBUF);
	    pending_chat_size = PKBUF;
	    pending_chat_len = 0;
	}
	if (pending_chat_len + MB_STRLEN >= pending_chat_size) {
	    char * p = realloc(pending_chat, pending_chat_size*2);
	    if (p == 0) {
		free(pending_chat);
		pending_chat = 0; pending_chat_size = pending_chat_len = 0;
		return;
	    }
	    pending_chat_size *= 2;
	}
	memcpy(pending_chat+pending_chat_len, msg, MB_STRLEN);
	pending_chat_len += MB_STRLEN;
	pending_chat[pending_chat_len] = 0;
	if (msg_flag == 1) return;
    }

    if (pending_chat_len) {
	pending_chat_len = 0;
	msg = pending_chat;
    }

    if (msg[0] == '/') {
	if (msg[1] != '/') {
	    run_command(msg);
	    return;
	}
	msg++;
    }

    convert_inbound_chat(msg);

    if (pending_chat_size >= 65536) {
	free(pending_chat);
	pending_chat = 0; pending_chat_size = pending_chat_len = 0;
    }
}

void
convert_inbound_chat(char * msg)
{
    char * buf = malloc(strlen(msg) + 256);
    char * p = buf + sprintf(buf, "&e%s:&f ", user_id);
    for(char *s = msg; *s; s++) {
	if (*s == '%' || *s == '&') {
	    char c = *s++;
	    if ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f'))
	    {
		*p++ = '&'; *p++ = *s;
		continue;
	    }
	    if (c == '%') *p++ = c;
	    if ((c != '%' || *s != '%') && *s != '&') *p++ = *s;
	} else {
	    *p++ = *s;
	}
    }
    *p = 0;

    post_chat(0, buf, p-buf);
    free(buf);
}

/* Post a long cp437 chat message to everyone (0) or just me (1) */
void
post_chat(int where, char * chat, int chat_len)
{
    int colour = -1, ncolour = 'e';
    pkt_message pkt;
    pkt.msg_flag = 0;
    if (chat_len <= 0) chat_len = strlen(chat);

    if (where == 0)
	log_chat_message(chat, chat_len);

    int s, d, ws = -1, wd = -1, add_gt = 0;
    for(d = s = 0; s<chat_len; s++) {
	uint8_t c = chat[s];

	// Colours are interpreted to be regenerated later.
	if (c == '&') {
	    uint8_t col = chat[s+1];

	    // MCGalaxy aliases --  // global chat colour &6
	    if (col >= 'A' && col <= 'F') col = col - 'A' + 'a';
	    if (col == 'H') col = 'e'; // HelpDescriptionColor
	    if (col == 'I') col = '5'; // IRCColor
	    if (col == 'S') col = 'e'; // DefaultColor
	    if (col == 'T') col = 'a'; // HelpSyntaxColor
	    if (col == 'W') col = 'c'; // WarningErrorColor

	    if ((col >= '0' && col <= '9') || (col >= 'a' && col <= 'f')) {
		ncolour = col;
		s++;
		continue;
	    }
	    c = 0xA8; // CP437 '¿'
	}

	c = c?c:' ';
	if (c == ' ') { ws = s; wd = d; }

	// If this won't fit, try to wordwrap.
	int needs = 1;
	if (colour != ncolour && c != ' ')
	    needs = 3;
	if (d+needs == MB_STRLEN) {
	    if (s+1 < chat_len && chat[s+1] != ' ') {
		if (wd > 0) { d = wd; s = ws+1; }
		while(d<MB_STRLEN) pkt.message[d++] = ' ';
	    }
	} else if (d+needs > MB_STRLEN) {
	    if (wd > 0) { d = wd; s = ws+1; }
	    while(d<MB_STRLEN) pkt.message[d++] = ' ';
	}

	// Add it in (if we haven't filled the buffer)
	if (d>=MB_STRLEN)
	    s--;
	else {
	    int eatsp = 0;
	    if (add_gt) {
		add_gt = 0;
		pkt.message[d++] = '&';
		pkt.message[d++] = 'f';
		pkt.message[d++] = '>';
		pkt.message[d++] = ' ';
		colour = 'f';
		eatsp = 1;
	    }
	    if (colour != ncolour && c != ' ') {
		pkt.message[d++] = '&';
		pkt.message[d++] = ncolour;
		colour = ncolour;
		pkt.message[d++] = c;
	    } else if (!eatsp || c != ' ')
		pkt.message[d++] = c;
	}

	// Full buffer gets sent.
	if (d >= MB_STRLEN) {
	    if (where == 1)
		send_msg_pkt_filtered(pkt.msg_flag, pkt.message);
	    else
		update_chat(&pkt);
	    d = 0;
	    add_gt = 1;
	    colour = -1;
	    ws = wd = -1;
	}
    }

    if (d>0) {
	while(d<MB_STRLEN) pkt.message[d++] = ' ';
	if (where == 1)
	    send_msg_pkt_filtered(pkt.msg_flag, pkt.message);
	else
	    update_chat(&pkt);
    }

    // If someone spams they get it all back.
    send_queued_chats();
}

/* Printf to the chat a pretty long message normally to just me.
 * Uses cp437 and clips at 4k. Prefix format with '@' to broadcast.
 */
void
printf_chat(char * fmt, ...)
{
    char pbuf[BUFSIZ];
    int to = 1;
    char *f = fmt;
    va_list ap;
    va_start(ap, fmt);
    if (*f == '@') { f++, to = 0; }
    int l = vsnprintf(pbuf, sizeof(pbuf), f, ap);
    if (l > sizeof(pbuf)) {
	strcpy(pbuf+sizeof(pbuf)-4, "...");
	l = sizeof(pbuf);
    }
    va_end(ap);

    post_chat(to, pbuf, l);
}
