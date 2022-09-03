#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>

#include "museum.h"

/*HELP museum H_CMD
&T/museum [levelname] [number] &Sor &T/museum [number]
Goto a backup of a level.
*/

#if INTERFACE
#define CMD_MUSEUM \
    {N"museum", &cmd_museum}
#endif

void
cmd_museum(char * UNUSED(cmd), char * arg)
{
    if (arg == 0 || *arg == 0) { cmd_help(0,"museum"); return; }

    char fixedname[MAXLEVELNAMELEN*4], buf2[256], levelname[MAXLEVELNAMELEN+1];

    char * lvl = 0, *levelid = 0;
    char * p = arg+strlen(arg);
    while (p>arg && p[-1] == ' ') *--p = 0;
    while (p>arg && p[-1] != ' ') --p;
    levelid = p;
    while (p>arg && p[-1] == ' ') *--p = 0;

    lvl = arg;
    while (lvl && *lvl == ' ') lvl++;
    if (lvl == levelid) lvl = 0;

    int backup_id = strtol(levelid, 0, 10);

    if (backup_id <= 0) {
	if (lvl && *lvl){
	    printf_chat("&SBackup %s for level '%s' does not exist", levelid, lvl);
	    return;
	}
	lvl = levelid;
	levelid = "1";
	backup_id = 1;
    }

    if (lvl) {
	int l = strlen(lvl);
	// A quoted name is used exactly, including spaces.
	if (l>2 && lvl[0] == '"' && lvl[l-1] == '"') {
	    lvl[l-1] = 0;
	    lvl++;
	    fix_fname(fixedname, sizeof(fixedname), lvl);
	} else
	    fix_fname(fixedname, sizeof(fixedname), lvl);
    } else {
	fix_fname(fixedname, sizeof(fixedname), current_level_name);
	lvl = current_level_name;
    }

    snprintf(buf2, sizeof(buf2), LEVEL_BACKUP_NAME, fixedname, backup_id);
    if (access(buf2, F_OK) != 0){
	fprintf_logfile("Backup file \"%s\" does not exist.", buf2);
	printf_chat("&SBackup %d for level %s does not exist", backup_id, lvl);
	return;
    }

    unfix_fname(levelname, sizeof(levelname), fixedname);
    if (*levelname == 0) {
	fprintf_logfile("Error on map name for \"/museum %s\" file:\"%s\"", lvl, fixedname);
	if (*lvl && !*fixedname)
	    printf_chat("&SNo levels match \"%s\"", lvl);
	else
	    printf_chat("&SCould not load level file \"%s\"", fixedname);
	return;
    }

    char fixedname2[MAXLEVELNAMELEN*4];
    strcpy(fixedname2, fixedname);

    snprintf(fixedname, sizeof(fixedname), "%s.%d", fixedname2, backup_id);

    stop_shared();

    start_level(levelname, fixedname, backup_id);
    open_level_files(levelname, backup_id, fixedname, 0);
    if (!level_prop) {
	printf_chat("&WLevel load failed, returning to main");
	cmd_main(0,0);
	return;
    }

    level_prop->readonly = 1;
    level_prop->disallowchange = 0;

    send_map_file();
    send_addentity_pkt(255, user_id, user_id, level_prop->spawn);

    printf_chat("@&S%s went to museum %d of &7%s", user_id, backup_id, levelname);

    // read_only_message();
}

