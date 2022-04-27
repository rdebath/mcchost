#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chat_message.h"

static char * pending_chat = 0;
static int pending_chat_size = 0;
static int pending_chat_len = 0;

void
convert_chat_message(int msg_flag, char * msg)
{
    char buf[256];

    if (msg_flag != -1) return; // TODO:

    // TODO: Message concat.

    if (msg[0] == '/') {
	if (msg[1] != '/') {
	    run_command(msg);
	    return;
	}
	msg++;
    }

    // if (col >= 'A' && col <= 'F') return (char)(col + ' ');
    // if (col == 'H') return Server.Config.HelpDescriptionColor[1];	&e
    // if (col == 'I') return Server.Config.IRCColor[1];		&5
    // if (col == 'S') return Server.Config.DefaultColor[1];		&e
    // if (col == 'T') return Server.Config.HelpSyntaxColor[1];		&a
    // if (col == 'W') return Server.Config.WarningErrorColor[1];	&c
    // global chat colour &6

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
	post_chat(buf, p-buf);
    } else {
	if (!pending_chat) {
	    pending_chat = malloc(PKBUF);
	    pending_chat_size = PKBUF;
	    pending_chat_len = 0;
	}

	strcpy(pending_chat, buf);
	pending_chat_len = p-buf;

	post_chat(pending_chat, pending_chat_len);
	pending_chat_len = 0;
    }
}

void
post_chat(char * chat, int chat_len)
{
    int colour = -1, s, d, ncolour = 'e';
    pkt_message pkt;
    pkt.msg_flag = 0;
    if (chat_len <= 0) chat_len = strlen(chat);

    write_logfile(chat, chat_len);

    for(d = s = 0; s<chat_len; s++) {
	char c = chat[s];
	if (c != '&') {
	    if (colour != ncolour && c != ' ' && c != 0) {
		if (d >= MB_STRLEN-3) {
		    s--;
		    while(d<MB_STRLEN) pkt.message[d++] = ' ';
		} else {
		    pkt.message[d++] = '&';
		    pkt.message[d++] = ncolour;
		    colour = ncolour;
		    pkt.message[d++] = c?c:' ';
		}
	    } else
		pkt.message[d++] = c?c:' ';
	} else {
	    char c = chat[s+1];
	    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
		ncolour = c;
	    s++;
	}
	if (d >= MB_STRLEN) {
	    update_chat(pkt);
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
	}
    }

    if (d>0) {
	while(d<MB_STRLEN) pkt.message[d++] = ' ';
	update_chat(pkt);
    }

    // If someone spams they get it all back.
    send_queued_chats();
}
