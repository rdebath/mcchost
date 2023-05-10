
#include <dirent.h>

#include "teleport.h"

#if INTERFACE
typedef struct summon_list_t summon_list_t;
struct summon_list_t {
    nbtstr_t name;
    uint8_t active;
    int player_id;
    int on_level;
    xyzhv_t posn;
};
#endif

pid_t level_loader_pid = 0;

int
direct_teleport(char *level, int backup_id, xyzhv_t *npos)
{
    char cw_pathname[PATH_MAX];
    char levelname[MAXLEVELNAMELEN+1];
    char levelstdname[MAXLEVELNAMELEN*4];

    if (!choose_level_files(level, backup_id, cw_pathname, levelname, levelstdname))
	return 0;

    if (level_prop && !server_runonce) {
	// Check to see if the level is loaded; return of 1 if it's gonna
	// happen in the background.
	if (preload_level(levelname, backup_id, levelstdname, cw_pathname) == 1)
	    return 0;
    }

    stop_shared();

    if (start_level(levelname, backup_id))
	open_level_files(levelname, backup_id, cw_pathname, levelstdname, 0);
    if (!level_prop) {
        printf_chat("&WLevel \"%s\" load failed, returning to main", level);
        open_main_level();
        return 0;
    }
    player_posn = level_prop->spawn;

    if (backup_id) {
	level_prop->readonly = 1;
	level_prop->disallowchange = 0;
    }

    if (!extn_extentityposn) {
	if (level_prop->cells_x>1024 || level_prop->cells_y>1024 || level_prop->cells_z>1024) {
	    if (strcmp(level, main_level()) != 0) {
		printf_chat("&WLevel \"%s\" is too large for your client", level);
		open_main_level();
		return 0;
	    }
	}
    }

    send_map_file(0);

    if (backup_id)
	printf_chat("@%s&S went to museum %d of &7%s", player_list_name.c, backup_id, levelname);
    else {
	printf_chat("@%s&S went to &7%s", player_list_name.c, levelname);
	read_only_message();
    }

    if (npos) {
	send_posn_pkt(-1, &player_posn, *npos);
	player_posn = *npos;
    }

    return 1;
}

int
choose_level_files(char *level, int backup_id, char *cw_pathname, char *levelname, char *levelstdname)
{
    char fixedname[MAXLEVELNAMELEN*4];

    fix_fname(fixedname, sizeof(fixedname), level);

    if (backup_id)
	snprintf(levelstdname, MAXLEVELNAMELEN*4, "%s.%d", fixedname, backup_id);
    else
	strcpy(levelstdname, fixedname);

    if (backup_id == 0) {
	if (!check_level(level, fixedname)) {
	    unfix_fname(levelname, MAXLEVELNAMELEN+1, fixedname);
	    printf_chat("&WLevel &S%s&W is not available.", levelname);
	    return 0;
	}

        snprintf(cw_pathname, PATH_MAX, LEVEL_CW_NAME, fixedname);
        if (access(cw_pathname, F_OK) != 0) {
            printf_chat("&SLevel \"%s\" does not exist", level);
            return 0;
        }

    } else if (backup_id > 0) {

	snprintf(cw_pathname, PATH_MAX, LEVEL_BACKUP_NAME, fixedname, backup_id);
	int flg = 1;
	// Normal name
	if (access(cw_pathname, F_OK) == 0) flg = 0;
	// Try no .N suffix for N==1
	if (flg && backup_id == 1) {
	    snprintf(cw_pathname, PATH_MAX, LEVEL_BACKUP_NAME_1, fixedname);
	    if (access(cw_pathname, F_OK) == 0) flg = 0;
	}
	if (flg) flg = !find_alt_museum_file(cw_pathname, PATH_MAX, level, backup_id);
	if (flg) {
	    snprintf(cw_pathname, PATH_MAX, LEVEL_BACKUP_NAME, fixedname, backup_id);
	    printlog("Backup file \"%s\" does not exist.", cw_pathname);
	    printf_chat("&SBackup %d for level \"%s\" does not exist", backup_id, level);
	    return 0;
	}

    } else
	return 0;

    unfix_fname(levelname, MAXLEVELNAMELEN+1, fixedname);
    if (*levelname == 0) {
	fprintf_logfile("Error on map name in direct_teleport \"%s\" file:\"%s\"", level, fixedname);
	if (*level && !*fixedname)
	    printf_chat("&SNo levels match \"%s\"", level);
	else
	    printf_chat("&SCould not load level file \"%s\"", fixedname);
	return 0;
    }

    return 1;
}

int
find_alt_museum_file(char * cw_pathname, int len, char * level, int backup_id)
{
    char fixedname_dir[MAXLEVELNAMELEN*4];
    char fixedname_file[MAXLEVELNAMELEN*4];

    char * sub = strchr(level, '/');
    if (sub == 0) {
	fix_fname(fixedname_dir, sizeof(fixedname_dir), level);
	fix_fname(fixedname_file, sizeof(fixedname_file), level);
    } else {
	*sub = 0;
	fix_fname(fixedname_dir, sizeof(fixedname_dir), level);
	fix_fname(fixedname_file, sizeof(fixedname_file), sub+1);
	*sub = '/';
    }
    if (!*fixedname_dir || !*fixedname_file) return 0;

    snprintf(cw_pathname, len, LEVEL_BACKUP_DIR_NAME "/%s/%s.%d.cw",
	fixedname_dir, fixedname_file, backup_id);

    if (access(cw_pathname, F_OK) == 0) return 1;

    if (backup_id != 1) return 0;

    snprintf(cw_pathname, len, LEVEL_BACKUP_DIR_NAME "/%s/%s.cw",
	fixedname_dir, fixedname_file);

    if (access(cw_pathname, F_OK) == 0) return 1;

    return 0;
}

/* RV: -1 => Just open the level (This routine can't or won't).
 * RV: 0 => Open the level; it's already loaded.
 * RV: 1 => Background job forked to load and summon.
 */
int
preload_level(char *levelname, int backup_id, char * levelstdname, char * cw_pathname)
{
    if (backup_id < 0) return -1;

    int level_id = -1;
    nbtstr_t level = {0};
    strcpy(level.c, levelname);

    for(int i=0; i<MAX_LEVEL; i++) {
	if (!shdat.client->levels[i].loaded) {
	    if (level_id == -1) level_id = i;
	    continue;
	}
	nbtstr_t n = shdat.client->levels[i].level;
	if (strcmp(n.c, level.c) == 0 && shdat.client->levels[i].backup_id == backup_id) {
	    // Level already loaded
	    return 0;
	}
    }
    if (level_id == -1) return -1; // Too many.

    if ((level_loader_pid = fork()) == 0) {
	if (line_ofd >= 0) close(line_ofd);
	if (line_ifd >= 0 && line_ofd != line_ifd) close(line_ifd);
	line_ifd = line_ofd = -1;

	stop_shared();
	*current_level_name = 0;
	current_level_backup_id = -1;

	direct_load_level(levelname, backup_id, levelstdname, cw_pathname, 0);
	stop_shared();
	exit(0);
    }
    if (level_loader_pid<0) {
        level_loader_pid = 0;
        perror("fork()");
	return -1;
    }

    return 1;
}

LOCAL void
direct_load_level(char *levelname, int backup_id, char * levelstdname, char * cw_pathname, summon_list_t * users_on)
{
    open_level_files(levelname, backup_id, cw_pathname, levelstdname, 0);
    if (!level_prop) {
	printf_chat("Level \"%s\" load failed", levelname);
	exit(0);
    }

    lock_fn(system_lock);

    int level_id = -1;
    nbtstr_t level = {0};
    strcpy(level.c, levelname);

    for(int i=0; i<MAX_LEVEL; i++) {
	if (!shdat.client->levels[i].loaded) {
	    if (level_id == -1) level_id = i;
	    continue;
	}
	nbtstr_t n = shdat.client->levels[i].level;
	if (strcmp(n.c, level.c) == 0 && shdat.client->levels[i].backup_id == backup_id) {
	    level_id = i;
	    break;
	}
    }

    if (level_id >= 0 && !shdat.client->levels[level_id].loaded) {
	client_level_t t = {0};
	t.level = level;
	t.loaded = 1;
	t.backup_id = backup_id;
	shdat.client->levels[level_id] = t;
	server->loaded_levels++;
    }

    if (users_on)
	summon_userlist(users_on, level_id);
    else if (my_user_no >= 0) {
	shdat.client->user[my_user_no].summon_level_id = level_id;
	shdat.client->user[my_user_no].summon_posn = level_prop->spawn;
    }

    unlock_fn(system_lock);
}

void
wait_for_forced_unload(char * lvlname, summon_list_t * users_on)
{
    lock_fn(system_lock);
    int level_id = -1;

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	nbtstr_t lv = shdat.client->levels[lvid].level;
	if (strcmp(lv.c, lvlname) == 0) {
	    level_id = lvid;
	    break;
	}
    }

    if (level_id < 0) {
	unlock_fn(system_lock);
	return;
    }

    if (users_on) {
	int uid = 0;
	for(int i=0; i<MAX_USER; i++)
	{
	    client_entry_t c = shdat.client->user[i];
	    if (!c.active) continue;
	    if (c.on_level != level_id) continue;
	    if (c.level_bkp_id != 0) continue;

	    users_on[uid].player_id = i;
	    users_on[uid].name = c.name;
	    users_on[uid].active = c.active;
	    users_on[uid].on_level = c.on_level;
	    users_on[uid].posn = c.posn;
	    if (users_on[uid].active)
		uid++;
	}
    }

    printf_chat("&SLevel '%s' is being unloaded", lvlname);
    shdat.client->levels[level_id].force_unload = 2;
    shdat.client->generation++;

    unlock_fn(system_lock);

    int x = 0;
    while(shdat.client->levels[level_id].loaded && shdat.client->levels[level_id].force_unload) {
	x = (x+1)%5;
	if (x == 0) shdat.client->generation++;
	msleep(100);
    }
}

void
return_users_to_level(char * level, summon_list_t * users_on)
{
    char fixedname[MAXLEVELNAMELEN*4];
    char cw_pathname[PATH_MAX];
    char levelname[MAXLEVELNAMELEN+1];
    char levelstdname[MAXLEVELNAMELEN*4];

    fix_fname(fixedname, sizeof(fixedname), level);
    strcpy(levelstdname, fixedname);

    saprintf(cw_pathname, LEVEL_CW_NAME, fixedname);
    if (access(cw_pathname, F_OK) != 0) {
        printf_chat("&SLevel \"%s\" does not exist", level);
        return;
    }

    unfix_fname(levelname, sizeof(levelname), fixedname);

    direct_load_level(levelname, 0, levelstdname, cw_pathname, users_on);
}

LOCAL void
summon_userlist(summon_list_t * users_on, int new_level_id)
{
    int locked = 0;
    for(int uid = 0; uid < MAX_USER; uid++)
    {
        if (!users_on[uid].active) break;
        if (!locked) { locked = 1; lock_fn(system_lock); }

        int id = users_on[uid].player_id;
        if (!shdat.client->user[id].active) continue;
        if (strcmp(shdat.client->user[id].name.c, users_on[uid].name.c) != 0)
            continue;

        shdat.client->user[id].summon_level_id = new_level_id>=0?new_level_id:users_on[uid].on_level;
        shdat.client->user[id].summon_posn = users_on[uid].posn;
    }
    if (locked) unlock_fn(system_lock);
}

