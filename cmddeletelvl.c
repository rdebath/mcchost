

#include "cmddeletelvl.h"

/*HELP deletelvl H_CMD
&T/deletelvl name
delete a level
*/

#if INTERFACE
#define CMD_DELETELVL {N"deletelvl", &cmd_deletelvl}
#endif

void
cmd_deletelvl(char * cmd, char * arg)
{
    char * lvlarg = arg;
    if (!lvlarg) return cmd_help(0, cmd);

    char levelname[256];
    saprintf(levelname, "%s", lvlarg);
    if (!perm_level_check(levelname, 1))
        return;

    char fixedname[MAXLEVELNAMELEN*4], buf2[256], lvlname[MAXLEVELNAMELEN+1];

    fix_fname(fixedname, sizeof(fixedname), levelname);
    unfix_fname(lvlname, sizeof(lvlname), fixedname);
    if (!*lvlname) {
	printf_chat("&WMap '%s' can not exist", levelname);
	return;
    }
    saprintf(buf2, LEVEL_CW_NAME, fixedname);
    if (access(buf2, F_OK) != 0) {
	printf_chat("&WMap '%s' does not exist", levelname);
	return;
    }

    lock_fn(system_lock);

    char hst_fn[256];
    next_backup_filename(hst_fn, sizeof(hst_fn), fixedname);

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	nbtstr_t lv = shdat.client->levels[lvid].level;
	if (strcmp(lv.c, levelname) == 0) {
	    printf_chat("&SLevel '%s' is currently loaded, unloading and deleting", levelname);
	    shdat.client->levels[lvid].force_unload = 1;
	    shdat.client->levels[lvid].delete_on_unload = 1;
	    unlock_fn(system_lock);
	    return;
	}
    }

    if (rename(buf2, hst_fn) < 0) {
	perror("Delete rename failed");
	unlock_fn(system_lock);
	printf_chat("&WLevel '%s' failed to delete", levelname);
	return;
    }

    unlock_fn(system_lock);

    printf_chat("&SLevel '%s' deleted", levelname);
}
