
#include "cmdrenamelvl.h"

/*HELP renamelvl H_CMD
&T/renamelvl oldname newname
rename a level
*/

#if INTERFACE
#define UCMD_RENAMELVL {N"renamelvl", &cmd_renamelvl, CMD_PERM_ADMIN, CMD_HELPARG}
#endif

void
cmd_renamelvl(char * UNUSED(cmd), char * arg)
{
    char * oldlvlarg = strarg(arg);
    char * newlvlarg = strarg(0);
    if (!oldlvlarg || !newlvlarg) {
	printf_chat("&WNeed both old and new level names");
	return;
    }

    char fixedname[MAXLEVELNAMELEN*4], lvlname[MAXLEVELNAMELEN+1];
    char fixedname2[MAXLEVELNAMELEN*4], lvlname2[MAXLEVELNAMELEN+1];
    char buf2[256], buf3[256];

    fix_fname(fixedname, sizeof(fixedname), oldlvlarg);
    fix_fname(fixedname2, sizeof(fixedname2), newlvlarg);

    unfix_fname(lvlname, sizeof(lvlname), fixedname);
    unfix_fname(lvlname2, sizeof(lvlname2), fixedname2);
    if (!*lvlname || !*lvlname2) { printf_chat("&WMap name is invalid"); return; }

    saprintf(buf2, LEVEL_CW_NAME, fixedname);
    if (access(buf2, F_OK) != 0) {
	char * newfixed = find_file_match(fixedname, oldlvlarg);

	if (newfixed != 0) {
	    saprintf(fixedname, "%s", newfixed);
	    free(newfixed);
	}
	else return;
    }
    unfix_fname(lvlname, sizeof(lvlname), fixedname);

    saprintf(buf2, LEVEL_CW_NAME, fixedname);
    if (access(buf2, F_OK) != 0) {
	printf_chat("&WMap '%s' does not exist", lvlname);
	return;
    }

    saprintf(buf3, LEVEL_CW_NAME, fixedname2);
    if (access(buf3, F_OK) == 0) {
	printf_chat("&WMap '%s' already exists", lvlname2);
	return;
    }

    if (do_direct_rename(lvlname, lvlname2) == 0)
        return;

    do_renamelvl(lvlname, lvlname2);
}

int
do_direct_rename(char * levelname, char * newlevelname)
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

    char fixedname[MAXLEVELNAMELEN*4];
    char fixedname2[MAXLEVELNAMELEN*4];
    fix_fname(fixedname, sizeof(fixedname), levelname);
    fix_fname(fixedname2, sizeof(fixedname2), newlevelname);

    char buf2[256], buf3[256];
    saprintf(buf2, LEVEL_CW_NAME, fixedname);
    saprintf(buf3, LEVEL_CW_NAME, fixedname2);

    if (rename(buf2, buf3) < 0) {
	perror("File rename failed");
	unlock_fn(system_lock);
	printf_chat("&WLevel '%s' failed to rename", levelname);
	return -1;
    }

    // Rename metadata file.
    saprintf(buf2, LEVEL_INI_NAME, fixedname);
    saprintf(buf3, LEVEL_INI_NAME, fixedname2);
    (void)rename(buf2, buf3);

    unlock_fn(system_lock);

    printf_chat("&SLevel '%s' renamed to '%s'", levelname, newlevelname);
    return 0;
}

int
do_renamelvl(char * levelname, char * newlevelname)
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

    summon_list_t * summon_list = calloc(MAX_USER, sizeof(summon_list_t));

    wait_for_forced_unload(levelname, summon_list);

    if (do_direct_rename(levelname, newlevelname) != 0) {
	printf_chat("&WLevel '%s' failed to rename", levelname);

	if (summon_list[0].active)
	    return_users_to_level(main_level(), summon_list);
    } else {
	if (summon_list[0].active)
	    return_users_to_level(newlevelname, summon_list);
    }

    exit(0);
}
