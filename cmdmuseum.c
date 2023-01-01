
#include "cmdmuseum.h"

/*HELP museum H_CMD
&T/museum [levelname] [number] &Sor &T/museum [number]
Goto a backup of a level.
*/

#if INTERFACE
#define CMD_MUSEUM \
    {N"museum", &cmd_museum, CMD_HELPARG}
#endif

void
cmd_museum(char * UNUSED(cmd), char * arg)
{
    char fixedname[MAXLEVELNAMELEN*4], buf2[256];

    char * lvl = 0, *levelid = 0;
    char * p = arg+strlen(arg);
    while (p>arg && p[-1] == ' ') *--p = 0;
    while (p>arg && p[-1] != ' ') --p;
    levelid = p;
    while (p>arg && p[-1] == ' ') *--p = 0;

    lvl = arg;
    while (lvl && *lvl == ' ') lvl++;
    if (lvl == levelid) lvl = 0;

    int backup_id = strtol(levelid, 0, 10);

    if (backup_id <= 0) {
	if (lvl && *lvl){
	    printf_chat("&SBackup %s for level '%s' does not exist", levelid, lvl);
	    return;
	}
	lvl = levelid;
	levelid = "1";
	backup_id = 1;
    }

    if (lvl) {
	int l = strlen(lvl);
	// A quoted name is used exactly, including spaces.
	if (l>2 && lvl[0] == '"' && lvl[l-1] == '"') {
	    lvl[l-1] = 0;
	    lvl++;
	    fix_fname(fixedname, sizeof(fixedname), lvl);
	} else
	    fix_fname(fixedname, sizeof(fixedname), lvl);
    } else {
	fix_fname(fixedname, sizeof(fixedname), current_level_name);
	lvl = current_level_name;
    }

    saprintf(buf2, LEVEL_BACKUP_NAME, fixedname, backup_id);
    if (access(buf2, F_OK) != 0){
	fprintf_logfile("Backup file \"%s\" does not exist.", buf2);
	printf_chat("&SBackup %d for level %s does not exist", backup_id, lvl);
	return;
    }

    direct_teleport(lvl, backup_id, 0);
}
