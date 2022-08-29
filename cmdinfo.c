#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cmdinfo.h"

/*HELP info H_CMD
&T/info
Information about a user
*/

#if INTERFACE
#define CMD_INFO \
    {N"info", &cmd_info}
#endif

void
cmd_info(char * UNUSED(cmd), char * UNUSED(arg))
{
    userrec_t *user = &my_user;
    if (current_level_backup_id == 0)
	printf_chat("%s is on %s at &a(%d,%d,%d)",
	    user->user_id, current_level_name,
	    player_posn.x/32, player_posn.y/32, player_posn.z/32);
    else if (current_level_backup_id>0)
	printf_chat("%s is on museum &a%d&S of %s at &a(%d,%d,%d)",
	    user->user_id, current_level_backup_id, current_level_name,
	    player_posn.x/32, player_posn.y/32, player_posn.z/32);
    else
	printf_chat("%s is nowhere.", user->user_id);

    printf_chat(" Has written &a%jd&S messages", (intmax_t)user->message_count);

    printf_chat(" Modified &a%jd&S\377blocks, &a%jd&S\377placed, &a%jd&S\377drawn, &a%jd&S\377deleted",
	(intmax_t)user->blocks_placed + user->blocks_deleted + user->blocks_drawn,
	(intmax_t)user->blocks_placed,
	(intmax_t)user->blocks_drawn,
	(intmax_t)user->blocks_deleted);

    char timebuf[256] = "";
    intmax_t s = user->time_online_secs;
    if (s > 86400) { sprintf(timebuf+strlen(timebuf), " %jdd", s/86400); s -= s/86400*86400; }
    if (s > 3600) { sprintf(timebuf+strlen(timebuf), " %jdh", s/3600); s -= s/3600*3600; }
    if (s > 60) { sprintf(timebuf+strlen(timebuf), " %jdm", s/60); s -= s/60*60; }
    if (s > 0 || user->time_online_secs == 0) sprintf(timebuf+strlen(timebuf), " %jds", s);

    printf_chat(" Spent&a%s&S on the server", timebuf);
    printf_chat(" Logged in &a%jd&S times, &a%jd&S of which ended in a kick",
	(intmax_t)user->logon_count, (intmax_t)user->kick_count);

    time_t f = user->first_logon, l = user->last_logon;
    struct tm first_log, last_log;
    gmtime_r(&f, &first_log);
    gmtime_r(&l, &last_log);

    printf_chat(" First login &a%04d-%02d-%02d&S, Last login &a%04d-%02d-%02d&S (UTC)",
	first_log.tm_year+1900, first_log.tm_mon+1, first_log.tm_mday,
	last_log.tm_year+1900, last_log.tm_mon+1, last_log.tm_mday);

    if (client_trusted)
	printf_chat(" IP address of &a%s", user->last_ip);

#if 0
    int64_t first_logon;        // is time_t, time_t is sometimes 32bits.
    int64_t last_logon;         // is time_t, time_t is sometimes 32bits.
    int64_t death_count;        // Currently unused
#endif
}
