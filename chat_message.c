#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

#include "chat_message.h"

static char * pending_chat = 0;
static int pending_chat_size = 0;
static int pending_chat_len = 0;

void
convert_chat_message(pkt_message pkt)
{
    char * msg = pkt.message;
    char buf[256];

    // TODO: Message concat.

    if (msg[0] == '/') {
	if (msg[1] != '/')
	{
	    if (msg[1] == 0) return; //TODO: Repeat command.

	    // TODO: Commands.
	    if (strcasecmp(msg, "/help") == 0) {
		send_message_pkt(0, "Help command: TODO ...");
		return;
	    }

	    if (strcasecmp(msg, "/quit") == 0) logout("Left the game.");
	    if (strcasecmp(msg, "/rq") == 0) logout("RAGEQUIT!!");

	    if (strcasecmp(msg, "/hax") == 0 || strcasecmp(msg, "/hacks") == 0)
		kicked("Your IP has been backtraced + reported to FBI Cyber Crimes Unit.");

	    if (strcasecmp(msg, "/crash") == 0)
		abort(), logout("Server crash! Error code 42");
	    if (strcasecmp(msg, "/reload") == 0)
	    {
		send_map_reload();
		//level_block_queue->generation += 2;
		return;
	    }

	    strcpy(buf, "&eUnknown command \"");
	    sscanf(msg+1, "%s", buf+strlen(buf));
	    buf[60] = 0;
	    strcat(buf, "\".");
	    send_message_pkt(0, buf);
	    return;
	}
	msg++;
    }

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
}
