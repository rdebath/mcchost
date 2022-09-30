#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

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
    {N"minfo", &cmd_minfo}, {N"mi", &cmd_minfo, .dup=1}, \
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
cmd_minfo(char * UNUSED(cmd), char * UNUSED(arg))
{
    time_t now = time(0);
    if (!level_prop) {
	printf_chat("&WThe void is eternal and unknowable");
	return;
    }

    if (current_level_backup_id > 0)
	printf_chat("&TAbout &WMuseum&S (&7%s %d): &SWidth=%d Height=%d Length=%d",
	    current_level_name, current_level_backup_id,
	    level_prop->cells_x, level_prop->cells_y, level_prop->cells_z);
    else if (current_level_backup_id == 0)
	printf_chat("&TAbout &7%s: &SWidth=%d Height=%d Length=%d",
	    current_level_name, level_prop->cells_x,
	    level_prop->cells_y, level_prop->cells_z);
    else
	printf_chat("&WAbout %s&S: &SWidth=%d Height=%d Length=%d",
	    current_level_name, level_prop->cells_x,
	    level_prop->cells_y, level_prop->cells_z);

    char timebuf[256], timebuf2[256];
    if (level_prop->time_created == 0)
	strcpy(timebuf, " (unknown)");
    else
	conv_duration(timebuf, now-level_prop->time_created);

    int last_bkp = -1;
    time_t last_backup_time = 0;
    {
	char fixname[MAXLEVELNAMELEN*4];
	fix_fname(fixname, sizeof(fixname), current_level_name);
	last_bkp = find_recent_backup(fixname, &last_backup_time);
    }
    char bkp_msg[512];
    if (last_bkp <= 0) strcpy(bkp_msg, "no backups yet");
    else {
	conv_duration(timebuf2, now-last_backup_time);
	saprintf(bkp_msg, "last backup (%s ago): &T%d",
	    timebuf2+1, last_bkp);
    }

    printf_chat("  Created%s ago, %s", timebuf, bkp_msg);
    if (level_prop->motd[0])
	printf_chat("  MOTD: &b%s", level_prop->motd);
    else
	printf_chat("  MOTD: &Wignore");

/*
About main: Width=128 Height=64 Length=128
  Physics are OFF, gun usage is disabled
  Created 43d 13h 55m ago, last backup (606d 10h 4m ago): 25
  BlockDB (Used for /b) is Enabled with 3735158 entries
  Visitable by Guest+
  Modifiable by Nobody+
Envionment settings
  No custom terrain set for this map.
  No custom texture pack set for this map.
  Colors: Fog none, Sky none, Clouds none, Sunlight none, Shadowlight none
  Water level: 33, Bedrock offset: -2, Clouds height:
> 66, Max fog distance: 0
  Edge Block: Stone, Horizon Block: Grass
  Clouds speed: 1.00%, Weather speed: 1.00%
  Weather fade rate: 1.00%, Exponential fog: OFF
  Skybox rotations: Horizontal none, Vertical none
Physics settings:
  Finite mode: OFF, Random flow: ON
  Finite high water OFF
  Animal hunt AI: ON, Edge water: OFF
  Grass growing: ON, Fern tree growing: OFF
  Leaf decay: OFF, Physics overload: 1500
  Physics speed: 250 milliseconds between ticks
Survival settings:
  Survival death: OFF (Fall: 9, Drown: 70)
  Guns: OFF, Killer blocks: ON
General settings:
  MOTD: ignore
  Local level only chat: OFF
  Load on /goto: ON, Auto unload: ON
  Buildable: OFF, Deletable: OFF, Drawing: ON
*/

}

#define HOURSECS (60*60)
#define DAYSECS (24*HOURSECS)
// Gregorian calendar year
#define YEARSECS (365*DAYSECS+2425*DAYSECS/10000)
#define MONTHSECS (YEARSECS/12)

void
conv_duration(char * timebuf, time_t duration)
{
    *timebuf = 0;
    int c = 0;
    intmax_t s = duration, s0 = s;
    // Note: This has a month and year as an exact number of seconds based on
    // the average length of the Gregorian calendar year. After going though
    // the leap year rules the exact year length is set as 365.2425 days.
    // The length of a month is then calculated as 30d 10h 29m 6s
    // (The measured value is about 365.2421875 but varies by leap seconds)
    // As it's not an exact number of days don't use it till over 99days.
    if (s > DAYSECS*99) {
	if (s > YEARSECS) { sprintf(timebuf+strlen(timebuf), " %jdy", s/YEARSECS); s -= s/YEARSECS*YEARSECS; c++;}
	if (s > MONTHSECS) { sprintf(timebuf+strlen(timebuf), " %jdmo", s/MONTHSECS); s -= s/MONTHSECS*MONTHSECS; c++;}
    }
    if (s > DAYSECS) { sprintf(timebuf+strlen(timebuf), " %jdd", s/DAYSECS); s -= s/DAYSECS*DAYSECS; c++;}
    if (c<3)
	if (s > HOURSECS) { sprintf(timebuf+strlen(timebuf), " %jdh", s/HOURSECS); s -= s/HOURSECS*HOURSECS; c++;}
    if (c<3)
	if (s > 60) { sprintf(timebuf+strlen(timebuf), " %jdm", s/60); s -= s/60*60; c++;}
    if (c<3)
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

int
find_recent_backup(char * matchstr, time_t * mtimep)
{
    struct dirent *entry;
    DIR *directory = opendir(LEVEL_BACKUP_DIR_NAME);
    if (!directory) return -1;

    int max_backup_id = -1;
    int mlen = 0;
    if (matchstr && *matchstr) mlen = strlen(matchstr);

    char bkpfname[MAXLEVELNAMELEN*4+sizeof(int)*3+9];

    while( (entry=readdir(directory)) )
    {
#if defined(_DIRENT_HAVE_D_TYPE) && defined(DT_REG) && defined(DT_UNKNOWN)
	if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN)
	    continue;
#endif
	int l = strlen(entry->d_name);
	if (l<=3 || strcmp(entry->d_name+l-3, ".cw") != 0) continue;

	char nbuf[MAXLEVELNAMELEN*4+sizeof(int)*3+3];
	char nbuf2[MAXLEVELNAMELEN+1];
	int backup_id = 0;

	l -= 3;
	if (l>sizeof(nbuf)-2) continue;
	memcpy(nbuf, entry->d_name, l);
	nbuf[l] = 0;

	char * p = strrchr(nbuf, '.');
	if (p == 0) continue;
	backup_id = atoi(p+1);
	if (backup_id<=0) continue;
	*p = 0;

	unfix_fname(nbuf2, sizeof(nbuf2), nbuf);
	if (*nbuf2 == 0) continue;
	l = strlen(nbuf2);
	if (l>MAXLEVELNAMELEN) continue;

	if (mlen)
	    if (strcmp(matchstr, nbuf2) != 0) continue;

	if (backup_id > max_backup_id) {
	    max_backup_id = backup_id;
	    strcpy(bkpfname, entry->d_name);
	}
    }
    closedir(directory);
    if (mtimep) {
        struct stat st;
	char buf[PATH_MAX];
	strcpy(buf, LEVEL_BACKUP_DIR_NAME);
	strcat(buf, "/");
	strcat(buf, bkpfname);
        if (stat(buf, &st) >= 0) *mtimep = st.st_mtime;
    }
    return max_backup_id;
}
