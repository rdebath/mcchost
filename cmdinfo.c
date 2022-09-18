#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>

#include "cmdinfo.h"

/*HELP info H_CMD
&T/info [Userid]
Show information about a user, default to yourself.
*/

/*HELP sinfo,serverinfo H_CMD
&T/sinfo
Show information about this server
Alias: &S/ServerInfo
*/

#if INTERFACE
#define CMD_INFO \
    {N"info", &cmd_info}, {N"i", &cmd_info, .dup=1}, \
    {N"sinfo", &cmd_sinfo}, {N"serverinfo", &cmd_sinfo, .dup=1}
#endif

void
cmd_info(char * UNUSED(cmd), char * arg)
{
    if (arg == 0)
	return show_user_info(&my_user);

    userrec_t user_rec[1] = {0};
    if (read_userrec(user_rec, arg, 1) >= 0)
	return show_user_info(user_rec);

    for(int i=0; i<MAX_USER; i++) {
	if (!shdat.client->user[i].active) continue;
	if (strstr(shdat.client->user[i].name.c, arg) == 0)
	    continue;

	if (read_userrec(user_rec, shdat.client->user[i].name.c, 1) >= 0)
	    return show_user_info(user_rec);
    }

    printf_chat("User \"%s\" not found", arg);
}

void
show_user_info(userrec_t *user)
{
    int is_me = 0;
    if (strcmp(user->user_id, user_id) == 0) {
	is_me = 1;
	if (current_level_backup_id == 0)
	    printf_chat("You on %s at &a(%d,%d,%d)", current_level_name,
		player_posn.x/32, player_posn.y/32, player_posn.z/32);
	else if (current_level_backup_id>0)
	    printf_chat("%s is on museum &a%d&S of %s at &a(%d,%d,%d)",
		user->user_id, current_level_backup_id, current_level_name,
		player_posn.x/32, player_posn.y/32, player_posn.z/32);
	else
	    printf_chat("%s is nowhere.", user->user_id);
    } else {
	int uid = -1, bkp_id = -1, level_id = -1;
	xyzhv_t posn;
	nbtstr_t level;
	if (shdat.client) {
	    for(int i=0; i<MAX_USER; i++) {
		if (!shdat.client->user[i].active) continue;
		if (strcmp(user->user_id, shdat.client->user[i].name.c) != 0)
		    continue;

		uid = i;
		level_id = shdat.client->user[i].on_level;
		bkp_id = shdat.client->user[i].level_bkp_id;
		posn = shdat.client->user[i].posn;
		if (level_id >= 0 && level_id < MAX_LEVEL)
		    level = shdat.client->levels[level_id].level;
		break;
	    }
	}

	if (uid < 0)
	    printf_chat("%s is not logged on", user->user_id);
	else {
	    if (bkp_id == 0 && level_id >= 0)
		printf_chat("%s is on %s at &a(%d,%d,%d)",
		    user->user_id, level.c, posn.x/32, posn.y/32, posn.z/32);
	    else if (level_id >= 0 && bkp_id >0)
		printf_chat("%s is on museum &a%d&S of %s at &a(%d,%d,%d)",
		    user->user_id, bkp_id, level.c,
		    posn.x/32, posn.y/32, posn.z/32);
	    else
		printf_chat("%s is nowhere.", user->user_id);
	}
    }

    printf_chat(" Has written &a%jd&S messages", (intmax_t)user->message_count);

    printf_chat(" Modified &a%jd&S\377blocks, &a%jd&S\377placed, &a%jd&S\377drawn, &a%jd&S\377deleted",
	(intmax_t)user->blocks_placed + user->blocks_deleted + user->blocks_drawn,
	(intmax_t)user->blocks_placed,
	(intmax_t)user->blocks_drawn,
	(intmax_t)user->blocks_deleted);

    char timebuf[256] = "";
    conv_duration(timebuf, user->time_online_secs);

    time_t logged = 0;
    if (is_me) logged = process_start_time(0);
    if (logged) {
	char timebuf2[256] = "";
	logged = time(0) - logged;
	conv_duration(timebuf2, logged);
	printf_chat(" Spent&a%s&S on the server,&a%s&S this session", timebuf, timebuf2);
    } else
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
}

void
cmd_sinfo(char * UNUSED(cmd), char * UNUSED(arg))
{
    time_t now = time(0);
    time_t then = process_start_time(alarm_handler_pid);
    printf_chat("About &T%s", server->name);

    // &T18&S players total. (&T1&S online)
    // &T51&S levels total (&T2&S loaded).

    if (then == 0)
	printf_chat(" Server start time not found");
    else {
	char timebuf[256];
	conv_duration(timebuf, now-then);
	printf_chat("  Been up for&T%s&S, running &T%s&f %s",
	    timebuf, server->software,
	    "https://github.com/rdebath/mcchost");
    }

    printf_chat("  Player positions are updated &T%g&S times/second",
	((int)(10000.0/server->player_update_ms))/10.0);

}

void
conv_duration(char * timebuf, time_t duration)
{
    *timebuf = 0;
    intmax_t s = duration, s0 = duration;
    if (s > 86400) { sprintf(timebuf+strlen(timebuf), " %jdd", s/86400); s -= s/86400*86400; }
    if (s > 3600) { sprintf(timebuf+strlen(timebuf), " %jdh", s/3600); s -= s/3600*3600; }
    if (s > 60) { sprintf(timebuf+strlen(timebuf), " %jdm", s/60); s -= s/60*60; }
    if (s > 0 || s0 <= 0) sprintf(timebuf+strlen(timebuf), " %jds", s);
}

time_t
process_start_time(int pid)
{
    int stat_fd;
    char stat_buf[2048];
    ssize_t read_result;

    long jiffies_per_second = sysconf(_SC_CLK_TCK);

    if (pid) {
	char procname[256];
	sprintf(procname, "/proc/%d/stat", pid);
	stat_fd = open(procname, O_RDONLY);
    } else
	stat_fd = open("/proc/self/stat",O_RDONLY);
    if(stat_fd<0) return 0;
    read_result = read(stat_fd,stat_buf,sizeof(stat_buf)-1);
    close(stat_fd);
    if(read_result<0) { fprintf(stderr,"/proc/pid/stat read() failed\n"); return 0; }
    stat_buf[read_result] = 0;

    uint64_t process_start_time_since_boot;
    int fno = 1; char *fld = strtok(stat_buf, " ");
    for(; 1; fno++, fld = strtok(0, " ")) {
	if (fno == 22) {
	    process_start_time_since_boot = !fld?0: strtol(fld, 0, 0);
	    break;
	}
    }

    stat_fd = open("/proc/stat",O_RDONLY);
    if(stat_fd<0) { fprintf(stderr,"/proc/stat open() failed\n"); return 0; }
    read_result = read(stat_fd,stat_buf,sizeof(stat_buf)-1);
    close(stat_fd);
    if(read_result<0) { fprintf(stderr,"/proc/stat read() failed\n"); return 0; }
    stat_buf[read_result] = 0;

    char * p = strstr(stat_buf,"btime ") + 6;
    if (!p) { fprintf(stderr, "Boot time not found\n"); return 0; }

    time_t process_start_time;
    uintmax_t boot_time = strtoimax(p, 0, 0);
    process_start_time = boot_time + process_start_time_since_boot / jiffies_per_second;

    return process_start_time;
}
