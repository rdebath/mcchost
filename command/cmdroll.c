
#include "cmdroll.h"

/*HELP roll H_CMD
&T/Roll [min] [max]
Rolls a random number between [min] and [max].
*/

#if INTERFACE
#define UCMD_ROLL {N"roll", &cmd_roll}
#endif

void
cmd_roll(char * UNUSED(cmd), char * arg)
{
    char * s_min = strarg(arg);
    char * s_max = strarg(0);

    int dmin = 1, dmax = 6;
    if (s_max == 0) {
	char * e = 0;
	dmax = strtol(s_min, &e, 10);
	if (*e) {
	    printf_chat("&W\"%s\" is not a valid integer", s_min);
	    return;
	}
    } else {
	char * e = 0;
	dmin = strtol(s_min, &e, 10);
	if (*e) {
	    printf_chat("&W\"%s\" is not a valid integer", s_min);
	    return;
	}
	dmax = strtol(s_max, &e, 10);
	if (*e) {
	    printf_chat("&W\"%s\" is not a valid integer", s_max);
	    return;
	}
    }

    if (dmin > dmax) { int t = dmin; dmin = dmax; dmax = t; }

    int v = bounded_random(dmax-dmin+1) + dmin;

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

    printf_chat("@&7%s&S rolled a &T%d&S (%d,%d)", user_id, v, dmin, dmax);
}
