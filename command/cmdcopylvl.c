
#include "cmdcopylvl.h"

/*HELP copylvl H_CMD
&T/copylvl oldname newname
Copy the current contents of level oldname to newname.
*/

#if INTERFACE
#define UCMD_COPYLVL {N"copylvl", &cmd_copylvl, CMD_HELPARG}
#endif

void
cmd_copylvl(char * UNUSED(cmd), char * arg)
{
    char * oldlvlarg = strarg(arg);
    char * newlvlarg = strarg(0);
    if (!oldlvlarg || !newlvlarg) {
	printf_chat("&WNeed both old and new level names");
	return;
    }

    if (!perm_level_check(oldlvlarg, 0, 0)) return;
    if (!perm_level_check(newlvlarg, 0, 0)) return;

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

    if (do_direct_copy(lvlname, lvlname2) == 0)
        return;

    do_copylvl(lvlname, lvlname2);
}

int
do_direct_copy(char * levelname, char * newlevelname)
{
    // If copying current level and it's clean, don't unload
    if (level_prop == 0 || level_prop->dirty_save != 0 ||
	!*current_level_name || strcmp(levelname, current_level_name) != 0)
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
	unlock_fn(system_lock);
    }

    char fixedname[MAXLEVELNAMELEN*4];
    char fixedname2[MAXLEVELNAMELEN*4];
    fix_fname(fixedname, sizeof(fixedname), levelname);
    fix_fname(fixedname2, sizeof(fixedname2), newlevelname);

    char buf2[256], buf3[256];
    saprintf(buf2, LEVEL_CW_NAME, fixedname);
    saprintf(buf3, LEVEL_CW_NAME, fixedname2);

    char tmp_fn[256];
    saprintf(tmp_fn, LEVEL_TMP_NAME, fixedname2);

    int txok = 1;
    FILE *ifd, *ofd;
    ifd = fopen(buf2, "r");
    if (ifd) {
	ofd = fopen(tmp_fn, "w");
	if (!ofd) txok = 0;
	else {
	    char buf[4096];
	    int c;
	    while(txok && (c=fread(buf, 1, sizeof(buf), ifd)) > 0)
		txok = (fwrite(buf, 1, c, ofd) == c);
	    fclose(ofd);
	}
	fclose(ifd);
    } else
	txok = 0;

    if (txok && rename(tmp_fn, buf3) < 0)
	txok = 0;

    if (!txok) {
	perror("Create copy and rename failed");
	int e = errno;
	(void) unlink(tmp_fn);
	errno = e;

	printf_chat("&WCopy of level \"%s\" failed", levelname);
	return -1;
    } else
	printf_chat("&SLevel '%s' copied to '%s'", levelname, newlevelname);

    return 0;
}

int
do_copylvl(char * levelname, char * newlevelname)
{
    if (level_processor_pid) return -1;
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

    if (do_direct_copy(levelname, newlevelname) != 0)
	printf_chat("&WLevel '%s' failed to copy", levelname);

    if (summon_list[0].active)
	return_users_to_level(levelname, summon_list);

    exit(0);
}
