
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
	 backup_id = strtol(lvl, &e, 10);
	 if (backup_id > 0 && *e == 0) {
	    levelid = lvl;
	    lvl = 0;
	 }
    } else if (levelid) {
	backup_id = strtol(levelid, &e, 10);
	if (*e != 0 && lvl) {
	    int try2 = strtol(lvl, &e, 10);
	    if (*e == 0) {
		char * t = levelid; levelid = lvl; lvl = t;
		backup_id = try2;
	    }
	}
    }

    if (backup_id <= 0) backup_id = 1;
    if (!lvl)
	lvl = current_level_name;

    direct_teleport(lvl, backup_id, 0);
}
