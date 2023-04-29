
#include "cmdrestorelvl.h"

/*HELP restore,restorelvl H_CMD
&T/restore [number]
Restores a previous backup for the level.
*/

#if INTERFACE
#define UCMD_RESTORE \
    {N"restore", &cmd_restorelvl}, \
    {N"restorelvl", &cmd_restorelvl, CMD_ALIAS}
#endif

void
cmd_restorelvl(char * UNUSED(cmd), char * arg)
{
    if (!arg || !*arg) {
#ifdef UCMD_MAPS
	return cmd_maps(0, "backups");
#else
	return printf_chat("&WUsage: /restore [backup_no]");
#endif
    }

    char * lvl = strarg(arg);
    char * levelid = strarg(0);
    char * e = 0;
    int backup_id = 0;

    if (levelid == 0 && lvl != 0) {
         backup_id = strtol(lvl, &e, 10);
         if (backup_id > 0 && *e == 0) {
            levelid = lvl;
            lvl = 0;
         }
    } else if (levelid) {
        backup_id = strtol(levelid, &e, 10);
        if (*e != 0 && lvl) {
            int try2 = strtol(lvl, &e, 10);
            if (*e == 0) {
                char * t = levelid; levelid = lvl; lvl = t;
                backup_id = try2;
            }
        }
    }

    if (backup_id <= 0) backup_id = 1;
    if (!lvl)
        lvl = current_level_name;

    if (!perm_level_check(lvl, 1, 0))
        return;

    if (level_processor_pid != 0) {
	printf_chat("&WYou are already processing a level; please try later");
	return;
    }

    char cw_pathname[PATH_MAX];
    char levelname[MAXLEVELNAMELEN+1];
    char levelstdname[MAXLEVELNAMELEN*4];

    if (!choose_level_files(lvl, backup_id, cw_pathname, levelname, levelstdname))
	return;

    restore_backup_file(lvl, backup_id, cw_pathname);
}

int
restore_backup_file(char * lvlname, int backup_id, char * source_cw_file)
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
    lvlname = strdup(lvlname); // Incase it's current_level_name

    if (line_ofd >= 0) close(line_ofd);
    if (line_ifd >= 0 && line_ofd != line_ifd) close(line_ifd);
    line_ifd = line_ofd = -1;

    stop_shared();
    *current_level_name = 0;
    current_level_backup_id = -1;

    summon_list_t * summon_list = calloc(MAX_USER, sizeof(summon_list_t));

    wait_for_forced_unload(lvlname, summon_list);

    char fixedname[MAXLEVELNAMELEN*4];
    char cw_pathname[PATH_MAX];
    fix_fname(fixedname, sizeof(fixedname), lvlname);
    saprintf(cw_pathname, LEVEL_CW_NAME, fixedname);

    copy_backup_to_map(source_cw_file, cw_pathname, lvlname, backup_id, fixedname);

    if (summon_list[0].active)
	return_users_to_level(lvlname, summon_list);
    exit(0);
}

LOCAL void
copy_backup_to_map(char * source, char * dest, char * lvlname, int backup_id, char * fixedname)
{
    int txok = 1;
    char tmp_fn[256];
    saprintf(tmp_fn, LEVEL_TMP_NAME, fixedname);

    FILE *ifd, *ofd;
    ifd = fopen(source, "r");
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

    if (txok && rename(tmp_fn, dest) < 0) {
	txok = 0;
    }
    if (!txok) {
	printf_chat("&WRestore failed on file copy for restore");
	perror("Restore copy and rename failed");
	int e = errno;
	(void) unlink(tmp_fn);
	errno = e;
    } else {
	fprintf_logfile("Restored backup %d for level \"%s\"", backup_id, lvlname);
	printf_chat("Restore of backup %d to \"%s\" succeeded", backup_id, lvlname);
    }
}
