
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
    char * s_count = strarg(0);

    int dmin = 1, dmax = 6, dcount = 1;
    unsigned long tmp;
    errno = 0;
    if (s_max == 0) {
	if (s_min) {
	    char * e = 0;
	    dmax = tmp = strtoul(s_min, &e, 10);
	    if (*e || errno == ERANGE || tmp > INT_MAX) {
		printf_chat("&W\"%s\" is not a valid value", s_min);
		return;
	    }
	}
    } else {
	char * e = 0;
	dmin = tmp = strtoul(s_min, &e, 10);
	if (*e || errno == ERANGE || tmp > INT_MAX) {
	    printf_chat("&W\"%s\" is not a valid value", s_min);
	    return;
	}
	dmax = tmp = strtoul(s_max, &e, 10);
	if (*e || errno == ERANGE || tmp > INT_MAX) {
	    printf_chat("&W\"%s\" is not a valid value", s_max);
	    return;
	}
    }

    if (s_count) {
	char * e = 0;
	dcount = tmp = strtoul(s_count, &e, 10);
	if (*e || errno == ERANGE || tmp > INT_MAX) {
	    printf_chat("&W\"%s\" is not a valid value", s_count);
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

    if (dcount < 1 || dcount > 1000) {
	printf_chat("@&7%s&S hurt themselves with a pile of dice", user_id);
	return;
    }

    if (dmin > dmax) { int t = dmin; dmin = dmax; dmax = t; }

    int v = bounded_random(dmax-dmin+1) + dmin;

    if (dcount > 1) {
	for(int i=1; i<dcount; i++)
	    v += bounded_random(dmax-dmin+1) + dmin;
    }

    if (dcount == 1) {
	if (dmin == 1) {
	    if (dmax == 6)
		printf_chat("@&7%s&S rolled a &T%d&S", user_id, v);
	    else
		printf_chat("@&7%s&S rolled a &T%d&S with a d%d", user_id, v, dmax);
	} else if (dmax == 99 && dmin == 0)
		printf_chat("@&7%s&S rolled &T%d%%&S", user_id, v);
	else
	    printf_chat("@&7%s&S rolled a &T%d&S in (%d..%d)", user_id, v, dmin, dmax);
    } else {
	if (dmin == 1) {
	    if (dmax == 6)
		printf_chat("@&7%s&S rolled a &T%d&S with %d dice", user_id, v, dcount);
	    else
		printf_chat("@&7%s&S rolled a &T%d&S with %d x d%d", user_id, v, dcount, dmax);
	} else
	    printf_chat("@&7%s&S rolled a &T%d&S with (%dx%d..%d)", user_id, v, dcount, dmin, dmax);
    }
}
