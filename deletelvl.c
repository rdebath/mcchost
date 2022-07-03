
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "deletelvl.h"

/*HELP deletelvl H_CMD
&T/deletelvl name
delete a level
*/

#if INTERFACE
#define CMD_DELETELVL {N"deletelvl", &cmd_deletelvl}
#endif

void
cmd_deletelvl(UNUSED char * cmd, char * arg)
{
    char * levelname = arg;

    if (!levelname) return cmd_help(0, cmd);

    char userlevel[256];
    snprintf(userlevel, sizeof(userlevel), "%s+", user_id);
    if (strcmp(levelname, "+") == 0 || strcmp(levelname, userlevel) == 0) {
	levelname = userlevel;
    } else if (!client_trusted) {
	printf_chat("&WPermission denied, your level name is '%s'", userlevel);
	return;
    }

    char fixedname[MAXLEVELNAMELEN*4], buf2[256], lvlname[MAXLEVELNAMELEN+1];

    fix_fname(fixedname, sizeof(fixedname), levelname);
    unfix_fname(lvlname, sizeof(lvlname), fixedname);
    if (!*lvlname) {
	printf_chat("&WMap '%s' can not exist", levelname);
	return;
    }
    snprintf(buf2, sizeof(buf2), LEVEL_CW_NAME, fixedname);
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
	    printf_chat("&WLevel '%s' is currently loaded", levelname);
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
