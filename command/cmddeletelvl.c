
#include "cmddeletelvl.h"

/*HELP deletelvl H_CMD
&T/deletelvl name
delete a level
*/

#if INTERFACE
#define UCMD_DELETELVL {N"deletelvl", &cmd_deletelvl,CMD_HELPARG}
#endif

void
cmd_deletelvl(char * UNUSED(cmd), char * arg)
{
    char * lvlarg = arg;

    char levelname[256];
    saprintf(levelname, "%s", lvlarg);
    if (!perm_level_check(levelname, 1, 0))
        return;

    char fixedname[MAXLEVELNAMELEN*4], buf2[256], lvlname[MAXLEVELNAMELEN+1];

    // Note: Only EXACT names allowed.

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

    if (do_direct_delete(levelname) == 0)
	return;

    do_deletelvl(levelname);
}

int
do_direct_delete(char * levelname)
{
    lock_fn(system_lock);

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
        if (!shdat.client->levels[lvid].loaded) continue;
        nbtstr_t lv = shdat.client->levels[lvid].level;
        if (strcmp(lv.c, levelname) == 0) {
            unlock_fn(system_lock);
	    return -1;
        }
    }

    char fixedname[MAXLEVELNAMELEN*4], buf2[256], buf3[256];
    fix_fname(fixedname, sizeof(fixedname), levelname);
    saprintf(buf2, LEVEL_CW_NAME, fixedname);

    char hst_fn[256];
    next_backup_filename(hst_fn, sizeof(hst_fn), fixedname);

    if (rename(buf2, hst_fn) < 0) {
	perror("Delete rename failed");
	unlock_fn(system_lock);
	printf_chat("&WLevel '%s' failed to delete", levelname);
	return -1;
    }

    saprintf(buf3, LEVEL_INI_NAME, fixedname);
    (void)unlink(buf3); // Remove additional data file.

    unlock_fn(system_lock);

    printf_chat("&SLevel '%s' deleted", levelname);
    return 0;
}

int
do_deletelvl(char * levelname)
{
    if (level_processor_pid || current_level_backup_id != 0) return -1;
    if ((level_processor_pid = fork()) != 0) {
        if (level_processor_pid<0) {
            level_processor_pid = 0;
            perror("fork()");
            return -1;
        }
        return 1;
    }

    if (line_ofd >= 0) close(line_ofd);
    if (line_ifd >= 0 && line_ofd != line_ifd) close(line_ifd);
    line_ifd = line_ofd = -1;

    wait_for_forced_unload(levelname, 0);

    if (do_direct_delete(levelname) != 0)
	printf_chat("&WLevel '%s' failed to delete", levelname);

    exit(0);
}
