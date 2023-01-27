
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
    char * lvl = 0, *levelid = 0;
    char * p = arg+strlen(arg);
    while (p>arg && p[-1] == ' ') *--p = 0;
    while (p>arg && p[-1] != ' ') --p;

    char * e = 0;
    int backup_id = strtol(p, &e, 10);
    if (!e || *e != 0) {
	backup_id = 1;
	levelid = "1";
    } else {
	levelid = p;
	while (p>arg && p[-1] == ' ') *--p = 0;
    }

    lvl = arg;
    while (lvl && *lvl == ' ') lvl++;
    if (lvl == levelid) lvl = 0;

    if (lvl) {
	int l = strlen(lvl);
	// A quoted name is used exactly, including spaces.
	if (l>2 && lvl[0] == '"' && lvl[l-1] == '"') {
	    lvl[l-1] = 0;
	    lvl++;
	}
    } else
	lvl = current_level_name;

    direct_teleport(lvl, backup_id, 0);
}
