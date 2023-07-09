#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "cmdtop.h"

/*HELP top H_CMD
&T/Top [max results] [stat] <offset>
Prints a list of players who have the most/top of a particular stat. Available stats:
Logins, Deaths, Oldest, Newest, Recent, Least-Recent, Kicked, Modified, Drawn, Placed, Deleted, TimeSpent, Messages
*/

/*HELP clones,alts H_CMD
&T/Clones [name]
Finds everyone with the same IP as [name]
&T/Clones [ip address]
Finds everyone who last played or is playing on the given IP
Alias: /alts, /WhoIP
*/

#if INTERFACE
#define UCMD_ALTS \
    {N"top", &cmd_top, CMD_HELPARG}, \
    {N"clones", &cmd_alts}, {N"alts", &cmd_alts, CMD_ALIAS}, \
    {N"whoip", &cmd_alts, CMD_ALIAS}
#endif

userrec_t * user_table = 0;
int user_table_size = 0;
int user_table_count = 0;

char * sort_names[] = {"Userid", "Logins", "Deaths", "Oldest",
    "Newest", "Recent", "Least-Recent", "Kicked", "Modified", "Drawn",
    "Placed", "Deleted", "TimeSpent", "Messages", 0};

char * sort_titles[] = { "",
    "Most logins", "Most deaths", "Oldest players", "Newest players",
    "Most recent players", "Least recent players", "Most times kicked",
    "Most blocks modified", "Most blocks drawn", "Most blocks placed",
    "Most blocks deleted", "Most time spent", "Most messages written"};

enum user_table_sort_t {
    s_uid, s_logins, s_deaths, s_oldest, s_newest, s_recent,
    s_leastrecent, s_kicked, s_modified, s_drawn, s_placed, s_deleted,
    s_timespent, s_messages, s_max
} user_table_sort = s_uid;

void
cmd_top(char * cmd, char * arg)
{
    char *as = strarg(arg);
    int top_count = 10, top_offset = 0;
    user_table_sort = s_uid;

    if (*as >= '0' && *as <= '9') { top_count = atoi(as); as = strarg(0); }
    if (as) for(int i = 0; sort_names[i]; i++) {
	if (strcasecmp(sort_names[i], as) == 0)
	    user_table_sort = i;
    }
    if (user_table_sort <= s_uid || user_table_sort >= s_max) return cmd_help(0, cmd);
    as = strarg(0);
    if (as) top_offset = atoi(as);

    load_user_table();
    printf_chat("&T%s", sort_titles[user_table_sort]);

    for(int i = top_offset; i<top_offset+top_count; i++) {
	if (i >= user_table_count) break;
	printf_chat("%d) &7%s&S - %s", i+1, user_table[i].user_id, top_stat_val(i));
    }
}

char *
top_stat_val(int uid)
{
static char statbuf[NB_SLEN];
    *statbuf = 0;
    uint64_t count = 0;
    time_t then = 0;
    int type = 0;

    switch(user_table_sort)
    {
	default: return "";

	case s_logins:
	    count = user_table[uid].logon_count;
	    break;
	case s_deaths:
	    count = user_table[uid].death_count;
	    break;
	case s_kicked:
	    count = user_table[uid].kick_count;
	    break;
	case s_modified:
	    count = user_table[uid].blocks_drawn + user_table[uid].blocks_placed + user_table[uid].blocks_deleted;
	    break;
	case s_drawn:
	    count = user_table[uid].blocks_drawn;
	    break;
	case s_placed:
	    count = user_table[uid].blocks_placed;
	    break;
	case s_deleted:
	    count = user_table[uid].blocks_deleted;
	    break;
	case s_messages:
	    count = user_table[uid].message_count;
	    break;

	case s_oldest:
	case s_newest:
	    then = user_table[uid].first_logon;
	    type = 1;
	    break;
	case s_recent:
	case s_leastrecent:
	    then = user_table[uid].last_logon;
	    type = 1;
	    break;
	case s_timespent:
	    count = user_table[uid].time_online_secs;
	    type = 2;
	    break;
    }

    switch(type)
    {
	case 0:
	{
	    int n = sizeof(uintmax_t)*4+4;
	    sprintf(statbuf, "%*jd", n, (intmax_t)count);
	    char * s = statbuf, *d = statbuf;
	    while(*s) {
		int ch = *s++; n--;
		if (ch == ' ') continue;
		if (d != statbuf && n%3 == 2) *d++ = ',';
		*d++ = ch;
	    }
	    *d = 0;
	    break;
	}
	case 1:
	{
	    strcpy(statbuf, "on ");
	    int l = strlen(statbuf);
	    if (my_user.timezone[0]) {
		setenv("TZ", my_user.timezone, 1);
		tzset();
		struct tm tm[1];
		localtime_r(&then, tm);
		strftime(statbuf+l, sizeof(statbuf)-l, "%d %b %Y %H:%M", tm);
		unsetenv("TZ");
		tzset();
	    } else {
		struct tm tm;
		gmtime_r(&then, &tm);

		sprintf(statbuf+l, "%04d-%02d-%02d",
		    tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
	    }

	    strcat(statbuf, ",");

	    time_t now = time(0);
	    count = now-then;
	    conv_duration(statbuf+strlen(statbuf), count);
	    strcat(statbuf, " ago");
	    break;
	}
	case 2:
	    conv_duration(statbuf, count);
	    break;
    }

    return statbuf;
}

void
cmd_alts(char * UNUSED(cmd), char * arg)
{
    userrec_t user_rec[1] = {0};
    if (*arg) {
	int is_ip = 0;
	if (*arg >= '0' && *arg <= '9') {
            struct in_addr ipaddr;
            if (inet_pton(AF_INET, arg, &ipaddr) == 1) {
		is_ip = 1;
		strncpy(user_rec->last_ip, arg, sizeof(user_rec->last_ip)-1);
	    }
	}

	if (!is_ip && read_userrec(user_rec, arg, 1) < 0) {
	    char user_name[128];
	    int i = find_player(arg, user_name, sizeof(user_name), 1, 0);
	    if (i<0) return;

	    if (read_userrec(user_rec, user_name, 1) < 0)
		return printf_chat("Details for user \"%s\" not found.", user_name);
	}
    } else *user_rec = my_user;

    load_user_table();

    printf_chat("These players have the same IP:");
    char linebuf[NB_SLEN*2] = "";
    int llen = 0, lc = 0;

    for(int i = 0; i<user_table_count; i++) {
	if (strcmp(user_rec->last_ip, user_table[i].last_ip) == 0) {
	    if (llen + 3 + strlen(user_table[i].user_id) > 64) {
		printf_chat("%s,", linebuf);
		llen = 0; lc++;
	    }
	    if (llen == 0) llen = sprintf(linebuf, "%s&7", lc++? "&f> ":"");
	    else llen += sprintf(linebuf+llen, ", ");
	    llen += sprintf(linebuf + llen, "%s", user_table[i].user_id);
	}
    }
    if (llen)
	printf_chat("%s", linebuf);
}

/* Use the "user" directory as the key because we want data that's
 * only stored in the files.
 */
int
load_user_table()
{
    int found = 0;
    DIR *directory = 0;
    struct dirent *entry;
    directory = opendir(USER_DIR);
    if (directory) {
	user_table_count = 0;
	while( (entry=readdir(directory)) )
	{
	    char *p, file_name[NB_SLEN*4];
	    if (strlen(entry->d_name) > sizeof(file_name)-4) continue;
	    strcpy(file_name, entry->d_name);
	    if ((p=strrchr(file_name, '.')) == 0) continue;
	    if (strcmp(p, ".ini") != 0) continue;
	    *p = 0;
	    char user_name[NB_SLEN];
	    decode_user_key(file_name, user_name, sizeof(user_name));

	    if (user_table_count >= user_table_size) {
		userrec_t * t = realloc(user_table, sizeof*user_table*(user_table_size+1));
		if (!t) break;
		user_table = t; user_table_size++;
	    }

	    if (read_userrec(user_table+user_table_count, user_name, 1) < 0)
		continue;
	    user_table_count++;
	    found++;
	}
	closedir(directory);

    } else {
	printf_chat("&WSearch of user directory failed");
	return -1;
    }

    if (found < 1)
	printf_chat("&WSearch of user directory returned no users");

    // Sort by user name.
    if (found > 1)
	qsort(user_table, user_table_count, sizeof(*user_table), pusertableentrycmp);

    return found;
}

#define UNWRAP(x,y) (((x) > (y)) - ((x) < (y)))

LOCAL int
pusertableentrycmp(const void *p1, const void *p2)
{
    userrec_t *e1 = (userrec_t *)p1;
    userrec_t *e2 = (userrec_t *)p2;

    switch(user_table_sort) {
    default:
	{
	    int r = strnatcasecmp(e1->user_id, e2->user_id);
	    if (r) return r;
	    /* If we get name duplicates, this stabilises the final order. */
	    r = strcmp(e1->user_id, e2->user_id);
	    return r;
	}

    case s_logins:
	return UNWRAP(e2->logon_count, e1->logon_count);
    case s_deaths:
	return UNWRAP(e2->death_count, e1->death_count);
    case s_oldest:
	return UNWRAP(e1->first_logon, e2->first_logon);
    case s_newest:
	return UNWRAP(e2->first_logon, e1->first_logon);
    case s_recent:
	return UNWRAP(e2->last_logon, e1->last_logon);
    case s_leastrecent:
	return UNWRAP(e1->last_logon, e2->last_logon);
    case s_kicked:
	return UNWRAP(e2->kick_count, e1->kick_count);
    case s_modified:
	return UNWRAP(e2->blocks_drawn+e2->blocks_placed+e2->blocks_deleted,
	              e1->blocks_drawn+e1->blocks_placed+e1->blocks_deleted);
    case s_drawn:
	return UNWRAP(e2->blocks_drawn, e1->blocks_drawn);
    case s_placed:
	return UNWRAP(e2->blocks_placed, e1->blocks_placed);
    case s_deleted:
	return UNWRAP(e2->blocks_deleted, e1->blocks_deleted);
    case s_timespent:
	return UNWRAP(e2->time_online_secs, e1->time_online_secs);
    case s_messages:
	return UNWRAP(e2->message_count, e1->message_count);
    }
}
