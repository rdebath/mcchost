
#include "cmdcopylvl.h"

/*HELP copylvl H_CMD
&T/copylvl oldname newname
copy a level
*/

#if INTERFACE
#define CMD_COPYLVL {N"copylvl", &cmd_copylvl, CMD_PERM_ADMIN}
#endif

void
cmd_copylvl(char * cmd, char * arg)
{
    char * oldlvlarg = strtok(arg, " ");
    char * newlvlarg = strtok(0, " ");
    if (!oldlvlarg || !newlvlarg) return cmd_help(0, cmd);

    char fixedname[MAXLEVELNAMELEN*4], buf2[256], lvlname[MAXLEVELNAMELEN+1];
    char fixedname2[MAXLEVELNAMELEN*4], buf3[256], lvlname2[MAXLEVELNAMELEN+1];

    fix_fname(fixedname, sizeof(fixedname), oldlvlarg);
    unfix_fname(lvlname, sizeof(lvlname), fixedname);
    fix_fname(fixedname2, sizeof(fixedname2), newlvlarg);
    unfix_fname(lvlname2, sizeof(lvlname2), fixedname2);

    if (!*lvlname || !*lvlname2) { printf_chat("&WMap name is invalid"); return; }

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

    saprintf(buf3, LEVEL_CW_NAME, fixedname2);
    if (access(buf3, F_OK) == 0) {
	printf_chat("&WMap '%s' already exists", lvlname2);
	return;
    }

    saprintf(buf2, LEVEL_CW_NAME, fixedname);
    if (access(buf2, F_OK) != 0) {
	printf_chat("&WMap '%s' does not exist", lvlname);
	return;
    }

    lock_fn(system_lock);

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	nbtstr_t lv = shdat.client->levels[lvid].level;
	if (strcmp(lv.c, lvlname) == 0) {
	    printf_chat("&SLevel '%s' is currently loaded", lvlname);
	    //shdat.client->levels[lvid].force_unload = 1;
	    //shdat.client->levels[lvid].delete_on_unload = 1;
	    unlock_fn(system_lock);
	    return;
	}
    }

    unlock_fn(system_lock);

    printf_chat("&SLevel '%s' would be copied to '%s'", lvlname, lvlname2);
}
