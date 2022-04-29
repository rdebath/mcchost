#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "chat_message.h"

static char * pending_chat = 0;
static int pending_chat_size = 0;
static int pending_chat_len = 0;

void
process_chat_message(int msg_flag, char * msg)
{
    if (msg_flag != -1) return; // TODO:

    // TODO: Message concat.

    if (msg[0] == '/') {
	if (msg[1] != '/') {
	    run_command(msg);
	    return;
	}
	msg++;
    }

    // if (msg_flag ... ) ?
    convert_chat_message(msg);
}

void
convert_chat_message(char * msg)
{

    char buf[256];
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

    if (pending_chat_len == 0 || pending_chat == 0) {
	post_chat(0, buf, p-buf);
    } else {
	if (!pending_chat) {
	    pending_chat = malloc(PKBUF);
	    pending_chat_size = PKBUF;
	    pending_chat_len = 0;
	}

	strcpy(pending_chat, buf);
	pending_chat_len = p-buf;

	post_chat(0, pending_chat, pending_chat_len);
	pending_chat_len = 0;
    }
}

/* Post a long chat message to everyone (0) or just me (1) */
void
post_chat(int where, char * chat, int chat_len)
{
    int colour = -1, ncolour = 'e';
    pkt_message pkt;
    pkt.msg_flag = 0;
    if (chat_len <= 0) chat_len = strlen(chat);

    if (where == 0)
	write_logfile(chat, chat_len);

    int s, d, ws = -1, wd = -1;
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
	    if (colour != ncolour && c != ' ') {
		pkt.message[d++] = '&';
		pkt.message[d++] = ncolour;
		colour = ncolour;
		pkt.message[d++] = c;
	    } else
		pkt.message[d++] = c;
	}

	// Full buffer gets sent.
	if (d >= MB_STRLEN) {
	    if (where == 1)
		send_msg_pkt_filtered(pkt.msg_flag, pkt.message);
	    else
		update_chat(&pkt);
	    d = 0;
	    pkt.message[d++] = '&';
	    pkt.message[d++] = 'f';
	    pkt.message[d++] = '>';
	    pkt.message[d++] = ' ';
	    if (colour != -1 && ncolour != 'f') {
		pkt.message[d++] = '&';
		pkt.message[d++] = ncolour;
		colour = ncolour;
	    }
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
