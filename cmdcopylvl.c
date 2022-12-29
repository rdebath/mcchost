
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
    char * lvlarg = arg?strtok(arg, " "):0;
    char * newlvlarg = arg?strtok(0, " "):0;
    if (!lvlarg || !newlvlarg) return cmd_help(0, cmd);

    char fixedname[MAXLEVELNAMELEN*4], buf2[256], lvlname[MAXLEVELNAMELEN+1];
    char fixedname2[MAXLEVELNAMELEN*4], buf3[256], lvlname2[MAXLEVELNAMELEN+1];

    fix_fname(fixedname, sizeof(fixedname), lvlarg);
    unfix_fname(lvlname, sizeof(lvlname), fixedname);
    fix_fname(fixedname2, sizeof(fixedname2), newlvlarg);
    unfix_fname(lvlname2, sizeof(lvlname2), fixedname2);

    if (!*lvlname || !*lvlname2) { printf_chat("&WMap name is invalid"); return; }

    saprintf(buf2, LEVEL_CW_NAME, fixedname);
    if (access(buf2, F_OK) != 0) {
	printf_chat("&WMap '%s' does not exist", lvlarg);
	return;
    }

#if 0
    lock_fn(system_lock);

    char hst_fn[256];
    next_backup_filename(hst_fn, sizeof(hst_fn), fixedname);

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	nbtstr_t lv = shdat.client->levels[lvid].level;
	if (strcmp(lv.c, lvlarg) == 0) {
	    printf_chat("&SLevel '%s' is currently loaded, unloading and deleting", lvlarg);
	    shdat.client->levels[lvid].force_unload = 1;
	    shdat.client->levels[lvid].delete_on_unload = 1;
	    unlock_fn(system_lock);
	    return;
	}
    }

    if (rename(buf2, hst_fn) < 0) {
	perror("Delete rename failed");
	unlock_fn(system_lock);
	printf_chat("&WLevel '%s' failed to delete", lvlarg);
	return;
    }

    unlock_fn(system_lock);
#endif

    printf_chat("&SLevel '%s' deleted", lvlarg);
}
