#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/utsname.h>

#include "cmdinfo.h"

/*HELP info,whois,whowas,i H_CMD
&T/info [Userid]
Show information about a user, default to yourself.
Aliases: &T/WhoWas, /WhoIs, /i
*/

/*HELP where H_CMD
&T/Where [name]
Displays level, position, and orientation of that player.
*/

/*HELP sinfo,serverinfo H_CMD
&T/sinfo
Show information about this server
Alias: &S/ServerInfo
*/

#if INTERFACE
#define UCMD_INFO \
    {N"info", &cmd_info}, {N"i", &cmd_info, CMD_ALIAS}, \
    {N"whois", &cmd_info, CMD_ALIAS}, {N"whowas", &cmd_info, CMD_ALIAS}, \
    {N"where", &cmd_where}, \
    {N"minfo", &cmd_minfo}, {N"mi", &cmd_minfo, CMD_ALIAS}, \
    {N"sinfo", &cmd_sinfo}, {N"serverinfo", &cmd_sinfo, CMD_ALIAS}
#endif

void
cmd_info(char * UNUSED(cmd), char * arg)
{
    if (*arg == 0)
	return show_user_info(&my_user);

    userrec_t user_rec[1] = {0};
    if (read_userrec(user_rec, arg, 1) >= 0)
	return show_user_info(user_rec);

    char user_name[128];
    int i = find_player(arg, user_name, sizeof(user_name), 1, 0);
    if (i<0) return;

    if (read_userrec(user_rec, user_name, 1) >= 0)
	return show_user_info(user_rec);
    printf_chat("Details for user \"%s\" not found.", user_name);
}

void
show_user_info(userrec_t *user)
{
    int is_me = (strcmp(user->user_id, user_id) == 0);

    display_location(user->user_id);

    char *perm_str = "unknown";
    if (user->user_group >= 0 && user->user_group < USER_PERM_CNT)
	perm_str = user_perms[user->user_group];
    if (is_me) {
	if (perm_is_admin())
	    perm_str = user_perms[0];
	else if (perm_level_check(0, 0, 1))
	    perm_str = "level admin";
    }

    printf_chat("; Rank of %s", perm_str);
    if (user->banned) printf_chat("; (banned)");
    printf_chat(", wrote &a%jd&S messages", (intmax_t)user->message_count);

    printf_chat(" Modified &a%jd&S\377blocks, &a%jd&S\377placed, &a%jd&S\377drawn, &a%jd&S\377deleted",
	(intmax_t)user->blocks_placed + user->blocks_deleted + user->blocks_drawn,
	(intmax_t)user->blocks_placed,
	(intmax_t)user->blocks_drawn,
	(intmax_t)user->blocks_deleted);

    char timebuf[256] = "";
    conv_duration(timebuf, user->time_online_secs);

    time_t logged = 0;
    if (is_me) {
	logged = process_start_time(0);
	if (!logged) logged = login_time;
    }
    if (logged) {
	char timebuf2[256] = "";
	logged = time(0) - logged;
	conv_duration(timebuf2, logged);
	printf_chat(" Spent&a%s&S on the server,&a%s&S this session", timebuf, timebuf2);
    } else
	printf_chat(" Spent&a%s&S on the server", timebuf);

    printf_chat(" Logged in &a%jd&S times, &a%jd&S of which ended in a kick",
	(intmax_t)user->logon_count, (intmax_t)user->kick_count);

    if (!my_user.timezone[0])
	detect_timezone();

    time_t f[1] = {user->first_logon}, l[1] = {user->last_logon};
    if (my_user.timezone[0]) {
        setenv("TZ", my_user.timezone, 1);
        tzset();
	struct tm tm[1];
	localtime_r(f, tm);
	char s[256];
	strftime(s, sizeof(s), "%a, %d %b %Y %T %z", tm);
	printf_chat(" First login &a%s", s);

	localtime_r(l, tm);
	strftime(s, sizeof(s), "%a, %d %b %Y %T %z", tm);
	printf_chat(" Last login &a%s", s);

        unsetenv("TZ");
        tzset();
    } else {
	struct tm first_log, last_log;
	gmtime_r(f, &first_log);
	gmtime_r(l, &last_log);

	printf_chat(" First login &a%04d-%02d-%02d&S, Last login &a%04d-%02d-%02d&S (UTC)",
	    first_log.tm_year+1900, first_log.tm_mon+1, first_log.tm_mday,
	    last_log.tm_year+1900, last_log.tm_mon+1, last_log.tm_mday);
    }

    if (perm_is_admin())
	printf_chat(" IP address of &a%s", user->last_ip);
}

void
display_location(char * user_name)
{
    if (strcmp(user_name, user_id) == 0) {
	if (my_user_no >= 128)
	    printf_chat("; %s &W[%d]&S is ", user_name, my_user_no);
	else
	    printf_chat("; %s is ", user_name);
	if (current_level_backup_id == 0)
	    printf_chat("on %s at &a(%d,%d,%d)",
		current_level_name,
		player_posn.x/32, player_posn.y/32, player_posn.z/32);
	else if (current_level_backup_id>0)
	    printf_chat("on museum &a%d&S of %s at &a(%d,%d,%d)",
		current_level_backup_id, current_level_name,
		player_posn.x/32, player_posn.y/32, player_posn.z/32);
	else
	    printf_chat("nowhere.");
    } else {
	int uid = -1, bkp_id = -1, level_id = -1;
	xyzhv_t posn;
	nbtstr_t level;
	if (shdat.client) {
	    for(int i=0; i<MAX_USER; i++) {
		if (!shdat.client->user[i].state.active) continue;
		if (strcmp(user_name, shdat.client->user[i].name.c) != 0)
		    continue;

		uid = i;
		user_name = shdat.client->user[i].name.c;
		level_id = shdat.client->user[i].state.on_level;
		bkp_id = shdat.client->user[i].level_bkp_id;
		posn = shdat.client->user[i].state.posn;
		if (level_id >= 0 && level_id < MAX_LEVEL)
		    level = shdat.client->levels[level_id].level;
		break;
	    }
	}

	printf_chat("; %s is ", user_name);
	if (uid < 0)
	    printf_chat("not logged on");
	else {
	    if (bkp_id == 0 && level_id >= 0)
		printf_chat("on %s at &a(%d,%d,%d)",
		    level.c, posn.x/32, posn.y/32, posn.z/32);
	    else if (level_id >= 0 && bkp_id >0)
		printf_chat("on museum &a%d&S of %s at &a(%d,%d,%d)",
		    bkp_id, level.c,
		    posn.x/32, posn.y/32, posn.z/32);
	    else
		printf_chat("nowhere.");
	}
    }
}

void
cmd_where(char * UNUSED(cmd), char * arg)
{
    if (*arg == 0)
        return display_location(user_id);

    char user_name[128];
    int i = find_player(arg, user_name, sizeof(user_name), 1, 0);
    if (i<0) return;

    display_location(user_name);
}

void
cmd_sinfo(char * UNUSED(cmd), char * UNUSED(arg))
{
    time_t now = time(0);
    time_t then = process_start_time(alarm_handler_pid);
    if (then == 0) then = proc_start_time;

    printf_chat("About &T%s", server->name);

    // &T18&S players total. (&T1&S online)
    // &T51&S levels total (&T2&S loaded).

    char timebuf[256];
    conv_duration(timebuf, now-then);
    printf_chat("  Been up for&T%s&S, running &T%s %s&f %s",
	timebuf, server->software, Version,
	"https://github.com/rdebath/mcchost");

    // for i in s m r v ; do echo "$i: $(uname -$i)"; done
    struct utsname name;
    if (uname (&name) != -1) {
	printf_chat("  OS Name: &T%s&S, Machine: &T%s", name.sysname, name.machine);
	printf_chat("  OS Release: &T%s", name.release);
	printf_chat("  OS Version: &T%s", name.version);
    }

    printf_chat("  Player positions are updated &T%g&S times/second",
	((int)(10000.0/server->player_update_ms))/10.0);
}

void
cmd_minfo(char * UNUSED(cmd), char * arg)
{
    int backup_id = current_level_backup_id;
    char * level_name = current_level_name;
    map_info_t * prop = level_prop;
    map_info_t prop_dat[1];

    if (arg[0]) {
	prop_dat[0] = (map_info_t){0};

	backup_id = 0;
	level_name = arg;
	prop = prop_dat;

	char fixname[MAXLEVELNAMELEN*4];
	char ini_name[MAXLEVELNAMELEN*4];
	fix_fname(fixname, sizeof(fixname), level_name);
	saprintf(ini_name, LEVEL_INI_NAME, fixname);

	level_ini_tgt = prop_dat;
	int l = load_ini_file(mcc_level_ini_fields, ini_name, 1, 0);
	level_ini_tgt = 0;
	if (l < 0) {
	    printf_chat("Level \"%s\" not found (%d)", level_name, l);
	    return;
	}
    }

    if (!prop) {
	printf_chat("&WThe void is eternal and unknowable");
	return;
    }

    if (backup_id > 0)
	printf_chat("&TAbout &WMuseum&S (&7%s %d&S): &SWidth=%d Height=%d Length=%d",
	    level_name, backup_id, prop->cells_x, prop->cells_y, prop->cells_z);
    else if (backup_id == 0)
	printf_chat("&TAbout &7%s&S: &SWidth=%d Height=%d Length=%d",
	    level_name, prop->cells_x, prop->cells_y, prop->cells_z);
    else
	printf_chat("&WAbout %s&S: &SWidth=%d Height=%d Length=%d",
	    level_name, prop->cells_x, prop->cells_y, prop->cells_z);

    time_t now = time(0);
    char timebuf[256], timebuf2[256];
    if (prop->time_created == 0)
	strcpy(timebuf, " (unknown)");
    else
	conv_duration(timebuf, now-prop->time_created);

    int last_bkp = -1;
    time_t last_backup_time = 0;
    {
	char fixname[MAXLEVELNAMELEN*4];
	fix_fname(fixname, sizeof(fixname), level_name);
	last_bkp = current_backup_file(0, 0, fixname, &last_backup_time);
    }
    char bkp_msg[512];
    if (last_bkp <= 0 || last_backup_time == 0)
	strcpy(bkp_msg, "no backups yet");
    else {
	conv_duration(timebuf2, now-last_backup_time);
	saprintf(bkp_msg, "last backup (%s ago): &T%d",
	    timebuf2+1, last_bkp);
    }

    printf_chat("  Created%s ago, %s", timebuf, bkp_msg);
    if (prop->motd[0])
	printf_chat("  MOTD: &b%s", prop->motd);
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
    char stat_buf[8192];
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
	    process_start_time_since_boot = !fld?0: strtoimax(fld, 0, 0);
	    break;
	}
    }

    stat_fd = open("/proc/stat",O_RDONLY);
    if(stat_fd<0) { fprintf(stderr,"/proc/stat open() failed\n"); return 0; }
    read_result = read(stat_fd,stat_buf,sizeof(stat_buf)-1);
    close(stat_fd);
    if(read_result<0) { fprintf(stderr,"/proc/stat read() failed\n"); return 0; }
    stat_buf[read_result] = 0;

    char * p = strstr(stat_buf,"btime ");
    if (!p) { fprintf(stderr, "Boot time not found '%s'\n", stat_buf); return 0; }
    p += 6;

    time_t process_start_time;
    uintmax_t boot_time = strtoimax(p, 0, 0);
    process_start_time = boot_time + process_start_time_since_boot / jiffies_per_second;

    return process_start_time;
}
