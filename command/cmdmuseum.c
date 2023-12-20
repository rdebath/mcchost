
#include "cmdmuseum.h"

/*HELP museum H_CMD
&T/museum [levelname] [number] &Sor &T/museum [number]
Goto a backup of a level.
*/

#if INTERFACE
#define UCMD_MUSEUM \
    {N"museum", &cmd_museum, CMD_HELPARG}
#endif

void
cmd_museum(char * UNUSED(cmd), char * arg)
{
    char * lvl = strarg(arg);
    char * levelid = strarg(0);
    char * e = 0;
    int backup_id = 0;

    if (levelid == 0 && lvl != 0) {
	 backup_id = strtoi(lvl, &e, 10);
	 if (backup_id > 0 && *e == 0) {
	    levelid = lvl;
	    lvl = 0;
	 } else
	    backup_id = 1;
    } else if (levelid) {
	backup_id = strtoi(levelid, &e, 10);
	if (*e != 0 && lvl) {
	    int try2 = strtoi(lvl, &e, 10);
	    if (*e == 0) {
		char * t = levelid; levelid = lvl; lvl = t;
		backup_id = try2;
	    }
	}
    }

    if (!lvl) lvl = current_level_name;

    // If backup 1 exists for this name assume the user has used an exact name.
    if (check_museum_name(lvl, backup_id) ||
	    (backup_id != 1 && check_museum_name(lvl, 1))) {
	direct_teleport(lvl, backup_id, 0);
	return;
    }

    // No exact match, see if it matches a level name.
    char fixedname[MAXLEVELNAMELEN*4], levelname[MAXLEVELNAMELEN];
    fix_fname(fixedname, sizeof(fixedname), lvl);

    // This matches LEVEL names not backup names.
    char * newfixed = find_file_match(fixedname, lvl);

    if (newfixed != 0) {
	unfix_fname(levelname, sizeof(levelname), newfixed);
	lvl = levelname;
	free(newfixed);
    } else
	return;

    // Try with the new level name.
    direct_teleport(lvl, backup_id, 0);
}

int
check_museum_name(char * level, int backup_id)
{
    char cw_pathname[PATH_MAX];
    char fixedname[MAXLEVELNAMELEN*4];
    fix_fname(fixedname, sizeof(fixedname), level);

    snprintf(cw_pathname, PATH_MAX, LEVEL_BACKUP_NAME, fixedname, backup_id);
    int flg = 1;
    // Normal name
    if (access(cw_pathname, F_OK) == 0) flg = 0;

    // Other names
    if (flg) flg = !find_alt_museum_file(cw_pathname, PATH_MAX, level, backup_id);

    return !flg;
}
