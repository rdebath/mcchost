
#include "chat_message.h"

static char * pending_chat = 0;
static int pending_chat_size = 0;
static int pending_chat_len = 0;

/* The arg msg is one 64byte chunk of a message in CP437 */
void
process_chat_message(int message_type, char * msg)
{
    // Message concat.
    if (message_type == 1 || pending_chat_len > 0) {
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
	    pending_chat = p;
	}
	memcpy(pending_chat+pending_chat_len, msg, MB_STRLEN);
	pending_chat_len += MB_STRLEN;
	pending_chat[pending_chat_len] = 0;
	if (message_type == 1) return;
    }

    if (pending_chat_len) {
	pending_chat_len = 0;
	msg = pending_chat;
    }

    // Msg is now a concatenated multi packet message in CP437
    // Note that Classicube will have converted '&' to '%'.
    int to_user = -1;

    if (!user_authenticated && (msg[0] != '/' || msg[1] == '/')) {
	printf_chat("You must verify using &T/pass [pass]&S first");
	log_chat_message(msg, strlen(msg), 0, 0, -1);
	return;
    }

    if (msg[0] == '/') {
	if (msg[1] != '/' && msg[1] != '@') {
	    revert_amp_to_perc(msg);
	    run_command(msg);
	    return;
	}
	msg++;
    } else if (msg[0] == '@') {
	char * user = strtok(msg+1, " ");
	msg = strtok(0, "");
	for(int i=0; i<MAX_USER; i++)
	{
	    client_entry_t c = shdat.client->user[i];
	    if (!c.active) continue;
	    if (strcmp(c.name.c, user) == 0) {
		to_user = i; break;
	    }
	}
	if (to_user<0) {
	    printf_chat("No online players equal \"%s\"", user);
	    return;
	}
    }

    if (add_antispam_event(player_chat_spam, server->chat_spam_count, server->chat_spam_interval, server->chat_spam_ban)) {
        int secs = 0;
        if (player_chat_spam->banned_til)
            secs = player_chat_spam->banned_til - time(0);
        if (secs < 2 || player_chat_spam->ban_flg == 0) {
	    player_chat_spam->ban_flg = 1;
	    printf_chat("You have been muted for spamming");
	} else
	    printf_chat("You are muted for %d more second%s", secs, secs==1?"":"s");
	return;
    }

    my_user.message_count++; my_user.dirty=1;

    update_player_move_time();

    convert_inbound_chat(to_user, msg);

    if (pending_chat_size >= 65536) {
	free(pending_chat);
	pending_chat = 0; pending_chat_size = pending_chat_len = 0;
    }

    // If someone spams they get it all back.
    send_queued_chats(1);
}

void
convert_inbound_chat(int to_user, char * msg)
{
    char * buf = malloc(strlen(msg) + 256);
    char * p;

    int to_level = -1;
    if (level_prop && level_prop->level_chat) {
	if (shdat.client && my_user_no >= 0 && my_user_no < MAX_USER)
	    to_level = shdat.client->user[my_user_no].on_level;
	if (to_level >= MAX_LEVEL)
	    to_level = -1;
    }

    if (to_user >= 0)
	p = buf + sprintf(buf, "&9[>] &e%s&e: &f", player_list_name.c);
    else if (to_level >= 0)
	p = buf + sprintf(buf, "&e<local>%s&e:&f ", player_title_name.c);
    else
	p = buf + sprintf(buf, "&e%s&e:&f ", player_title_name.c);

    for(char *s = msg; *s; s++) {
	if (!extn_textcolours && (*s == '%' || *s == '&')) {
	    char c = *s++;
	    if ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f'))
	    {
		*p++ = '&'; *p++ = *s;
		continue;
	    }
	    if (c == '%') *p++ = c;
	    if ((c != '%' || *s != '%') && *s != '&') *p++ = *s;
	} else {
	    if (*s == '%') { *p++ = '&'; s++; } // Classicube mangles &
	    *p++ = *s;
	}
    }
    *p = 0;
    int l = p-buf+1;
    convert_from_paren(buf, &l);

    if (to_user>= 0) {
	post_chat(1, to_user, 0, buf, l-1);
	buf[1] = 'S'; buf[3] = '<';
	post_chat(-1, 0, 0, buf, l-1);
    } else if (to_level >= 0)
	post_chat(2, to_level, 0, buf, l-1);
    else
	post_chat(0, 1, 0, buf, l-1);
    free(buf);
}

/* The '&' character is used for colours but Classicube changes
 * all '&' to '%'. So I change '%' back to '&' but check if it's
 * part of a valid colour combination; if not I change to '%'.
 */
void
revert_amp_to_perc(char * msg)
{
    for(char * p = msg; *p; p++) {
	if (*p == '%' || *p == '&') {
	    uint8_t c = p[1];
	    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
		*p++ = '&';
	    } else if (c && textcolour[c].defined) {
		*p++ = '&';
		if (textcolour[c].colour < 0)
		    *p = textcolour[c].fallback;
	    } else
		*p = '%';
	}
    }
}

/* Post a long cp437 chat message */
/* where == -1 => Just me.
 * where: 0) all, 1) Player(to_id), 2) Level(to_id), 3) Team(to_id) */
void
post_chat(int where, int to_id, int type, char * chat, int chat_len)
{
    int colour = 'f', ncolour = 'e';
    pkt_message pkt = {.message_type = type};
    if (chat_len <= 0) chat_len = strlen(chat);

    if (chat_len <= 0 || chat[0] <= ' ' || chat[0] > '~')
	colour = -1;

    if (where >= 0)
	log_chat_message(chat, chat_len, where, to_id, type);
    else if (!user_logged_in)
	return; // Would go to everyone eventually.

    if (where == -1 && line_ofd <= 0 ) {
	// There's no connection; this should be a background job.
	if (my_user_no >= 0)
	    where = 1, to_id = my_user_no;
	else
	    where = to_id = 0;
    }

    int s, d, ws = -1, wd = -1, add_gt = 0, el = 1;
    for(d = s = 0; s<chat_len; s++) {
	uint8_t c = chat[s];

	// Colours are interpreted to be regenerated later.
	if (c == '&') {
	    uint8_t col = chat[s+1];

	    // No while loop here, user could make a fallback loop.
	    if (textcolour[col].defined && textcolour[col].colour < 0)
		    col = textcolour[col].fallback;

	    if (textcolour[col].defined ||
		    (col >= '0' && col <= '9') || (col >= 'a' && col <= 'f')) {
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
	if (d==0 && c <= ' ')
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
		if (colour != 'f') {
		    pkt.message[d++] = '&';
		    pkt.message[d++] = 'f';
		    colour = 'f';
		}
		pkt.message[d++] = '>';
		pkt.message[d++] = ' ';
		eatsp = 1;
	    }
	    if (needs>1 && colour != ncolour) {
		pkt.message[d++] = '&';
		pkt.message[d++] = ncolour;
		colour = ncolour;
		pkt.message[d++] = c;
	    } else if (!eatsp || c != ' ')
		pkt.message[d++] = c;
	}

	// Full buffer gets sent IF it's a normal message
	// Otherwise, truncate.
	if (d >= MB_STRLEN) {
	    if (type == 0) {
		if (where == -1)
		    send_message_pkt(0, pkt.message_type, pkt.message);
		else
		    update_chat(where, to_id, &pkt);
	    } else
		break;

	    el = d = 0;
	    add_gt = 1;
	    colour = 'f';
	    if (chat[s]!=0 && (chat[s] < ' ' || chat[s] > '~')) colour = -1;
	    ws = wd = -1;
	}
    }

    if (d>0 || el) {
	while(d<MB_STRLEN) pkt.message[d++] = ' ';
	if (where == -1)
	    send_message_pkt(0, pkt.message_type, pkt.message);
	else
	    update_chat(where, to_id, &pkt);
    }
}

/* Printf to the chat a pretty long message normally to just me.
 * Uses cp437 and clips at 16k.
 *
 * Prefix format with '@' to broadcast.
 * Prefix format with '#' for UTF-8
 * Prefix format with '#@' to broadcast UTF-8
 * Prefix format with ';' to save for next chat
 * Prefix format with '\\' to quote a prefix.
 * Prefix format with '(11)' to choose message type
 * Prefix format with '~' for '%' -> '&' conversion.
 */

#if INTERFACE
#define printf_chat_w __attribute__ ((format (printf, 1, 2)))
#endif

void printf_chat_w
printf_chat(char * fmt, ...)
{
static char * saved_buffer = 0;

    char pbuf[16<<10];
    int to = -1, utf8 = 0, type = 0, percamp = 0, save_it = 0;
    char *f = fmt;
    va_list ap;
    va_start(ap, fmt);
    for(;;) {
	if (*f == '#') { f++, utf8 = 1; }
	else if (*f == '@') { f++, to = 0; }
	else if (*f == '~') { f++, percamp = 1; }
	else if (*f == ';') { f++, save_it = 1; }
	else if (*f == '(') {
	    f++;
	    type = atoi(f);
	    if(type) {
		while(*f >= '0' && *f <= '9') f++;
		if (*f == ')') f++;
	    }
	}
	else if (*f == '\\') { f++; break; }
	else break;
    }
    int l = vsnprintf(pbuf, sizeof(pbuf), f, ap);
    if (l > sizeof(pbuf)) {
	strcpy(pbuf+sizeof(pbuf)-4, "...");
	l = sizeof(pbuf);
    }
    va_end(ap);

    if (percamp) {
	char * p = pbuf;
	for(;*p; p++)
	    if (*p == '%') {
		int col = p[1];
		if ((col >= '0' && col <= '9') || (col >= 'a' && col <= 'f'))
		    *p = '&';
	    }
    }

    if (utf8)
	convert_to_cp437(pbuf, &l);

    if (save_it || saved_buffer) {
	int ol = (saved_buffer?strlen(saved_buffer):0); 
	char * np = malloc(l+ol+1);
	memcpy(np, saved_buffer, ol);
	memcpy(np+ol, pbuf, l);
	np[ol+l] = 0;
	if (saved_buffer) free(saved_buffer);
	if (save_it) {
	    saved_buffer = np;
	} else {
	    saved_buffer = 0;
	    post_chat(to, 0, type, np, ol+l);
	    free(np);
	}
    } else
	post_chat(to, 0, type, pbuf, l);
}
