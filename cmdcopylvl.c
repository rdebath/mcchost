
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

    char fixedname[MAXLEVELNAMELEN*4], lvlname[MAXLEVELNAMELEN+1];
    char fixedname2[MAXLEVELNAMELEN*4], lvlname2[MAXLEVELNAMELEN+1];
    char buf2[256], buf3[256], tmp_fn[256];

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

    lock_fn(system_lock);

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	nbtstr_t lv = shdat.client->levels[lvid].level;
	if (strcmp(lv.c, lvlname) == 0) {
	    printf_chat("&SNote: Level '%s' is currently loaded, using last save", lvlname);
	    break;
	}
    }

    unlock_fn(system_lock);

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
	perror("Backup copy and rename failed");
	int e = errno;
	(void) unlink(tmp_fn);
	errno = e;

	printf_chat("&WCopy of level \"%s\" failed", lvlname);
    } else
	printf_chat("&SLevel '%s' copied to '%s'", lvlname, lvlname2);
}
