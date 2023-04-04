
#include <dirent.h>

#include "teleport.h"

int
direct_teleport(char *level, int backup_id, xyzhv_t *npos)
{
    char fixedname[MAXLEVELNAMELEN*4];
    char cw_pathname[PATH_MAX];
    char levelname[MAXLEVELNAMELEN+1];
    char levelstdname[MAXLEVELNAMELEN*4];

    fix_fname(fixedname, sizeof(fixedname), level);

    if (backup_id)
	saprintf(levelstdname, "%s.%d", fixedname, backup_id);
    else
	strcpy(levelstdname, fixedname);

    if (backup_id == 0) {
	if (!check_level(level, fixedname)) {
	    unfix_fname(levelname, sizeof(levelname), fixedname);
	    printf_chat("&WLevel &S%s&W is not available.", levelname);
	    return 0;
	}

        saprintf(cw_pathname, LEVEL_CW_NAME, fixedname);
        if (access(cw_pathname, F_OK) != 0) {
            printf_chat("&SLevel \"%s\" does not exist", level);
            return 0;
        }

    } else if (backup_id > 0) {

	saprintf(cw_pathname, LEVEL_BACKUP_NAME, fixedname, backup_id);
	int flg = 1;
	// Normal name
	if (access(cw_pathname, F_OK) == 0) flg = 0;
	// Try no .N suffix for N==1
	if (flg && backup_id == 1) {
	    saprintf(cw_pathname, LEVEL_BACKUP_NAME_1, fixedname);
	    if (access(cw_pathname, F_OK) == 0) flg = 0;
	}
	if (flg) flg = !find_alt_museum_file(cw_pathname, sizeof(cw_pathname), level, backup_id);
	if (flg) {
	    saprintf(cw_pathname, LEVEL_BACKUP_NAME, fixedname, backup_id);
	    printlog("Backup file \"%s\" does not exist.", cw_pathname);
	    printf_chat("&SBackup %d for level \"%s\" does not exist", backup_id, level);
	    return 0;
	}

    } else
	return 0;

    unfix_fname(levelname, sizeof(levelname), fixedname);
    if (*levelname == 0) {
	fprintf_logfile("Error on map name in direct_teleport \"%s\" file:\"%s\"", level, fixedname);
	if (*level && !*fixedname)
	    printf_chat("&SNo levels match \"%s\"", level);
	else
	    printf_chat("&SCould not load level file \"%s\"", fixedname);
	return 0;
    }

    stop_shared();

    if (start_level(levelname, levelstdname, backup_id))
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

    send_map_file();

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
