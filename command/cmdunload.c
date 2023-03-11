#include <signal.h>

#include "cmdunload.h"

/*HELP unload H_CMD
&T/unload name
Save a level to it's compressed format and unload it.
*/

#if INTERFACE
#define CMD_UNLOADLVL {N"unload", &cmd_unload}
#endif

void
cmd_unload(char * UNUSED(cmd), char * arg)
{
    char * lvlarg = arg;

    char levelname[256];
    if (!arg || !*arg)
	strcpy(levelname, current_level_name);
    else
	saprintf(levelname, "%s", lvlarg);
    if (!perm_level_check(levelname, 1, 0))
        return;

    char fixedname[MAXLEVELNAMELEN*4], lvlname[MAXLEVELNAMELEN+1];

    fix_fname(fixedname, sizeof(fixedname), levelname);
    unfix_fname(lvlname, sizeof(lvlname), fixedname);
    if (!*lvlname) {
	printf_chat("&SLevel '%s' is not loaded", arg?arg:"");
	return;
    }

    if (strcmp(lvlname, current_level_name) != 0) {
	char buf2[256];
	saprintf(buf2, LEVEL_CW_NAME, fixedname);
	if (access(buf2, F_OK) != 0) {
	    char * newfixed = find_file_match(fixedname, arg);

	    if (newfixed != 0) {
		saprintf(fixedname, "%s", newfixed);
		free(newfixed);
	    }
	    else return;
	}
	unfix_fname(lvlname, sizeof(lvlname), fixedname);
    }

    lock_fn(system_lock);

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	nbtstr_t lv = shdat.client->levels[lvid].level;
	if (strcmp(lv.c, lvlname) == 0) {
	    printf_chat("&SLevel '%s' is unloading", lvlname);
	    shdat.client->levels[lvid].force_unload = 1;
	    shdat.client->generation++;
	    unlock_fn(system_lock);
	    if (alarm_handler_pid)
		kill(alarm_handler_pid, SIGALRM);
	    return;
	}
    }

    unlock_fn(system_lock);

    printf_chat("&SLevel '%s' is not loaded", levelname);
}
