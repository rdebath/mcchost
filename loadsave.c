#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "loadsave.h"
/*HELP goto
&T/goto [levelname]
Switch you current level
*/
/*HELP main
&T/main
Return to the system main level
*/

/*TODO: HELP load
&T/load [name]
*/
/*TODO: HELP save
&T/save [name]
*/

#if INTERFACE
#define CMD_LOADSAVE \
    {N"load", &cmd_load}, {N"save", &cmd_save}, \
    {N"goto", &cmd_goto}, {N"g", &cmd_goto, .dup=1}, \
    {N"main", &cmd_main}
#endif

static inline int E(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }

void
cmd_goto(UNUSED char * cmd, char * arg)
{
    char fixedname[NB_SLEN], buf2[256];
    fix_fname(fixedname, sizeof(fixedname), arg);
    snprintf(buf2, sizeof(buf2), "map/%s.cw", fixedname);
    if (access(buf2, F_OK) != 0) {
	snprintf(buf2, sizeof(buf2), "map/%s.ini", fixedname);
	if (access(buf2, F_OK) != 0) {
	    printf_chat("&SNo levels match \"%s\"", arg);
	    return;
	}
    }

    stop_shared();

    open_level_files(fixedname, 0);
    if (!level_prop) {
	printf_chat("&WLevel load failed, returning to main");
	cmd_main("","");
	return;
    }
    start_level(fixedname);
    send_map_file();
    send_spawn_pkt(255, user_id, level_prop->spawn);

    if (level_prop->readonly)
	printf_chat("&WLoaded read only map, changes will be discarded");
}

void
cmd_main(UNUSED char * cmd, UNUSED char * arg)
{
    char fixedname[NB_SLEN];
    fix_fname(fixedname, sizeof(fixedname), server.main_level);

    stop_shared();

    open_level_files(fixedname, 0);
    if (!level_prop)
	fatal("Failed to load main.");
    start_level(fixedname);
    send_map_file();
    send_spawn_pkt(255, user_id, level_prop->spawn);
}

void
cmd_load(UNUSED char * cmd, char * arg)
{
    if (!client_ipv4_localhost)
	printf_chat("&WUsage: /load [Auth] filename");
    else if (!arg || !*arg)
	printf_chat("&WUsage: /load filename");
    else {
	char buf1[256], buf2[256], *s, *d;
	for(s=arg, d=buf1; *s && d<buf1+sizeof(buf1)-1; s++) {
	    if ((*s>='A' && *s<='Z') || (*s>='a' && *s<='z') ||
		(*s>='0' && *s<='9') || *s=='_' || *s=='.')
		*d++ = *s;
	}
	*d = 0;

	snprintf(buf2, sizeof(buf2), "backup/%.200s.ini", buf1);

	lock_shared();
	int x = level_prop->cells_x;
	int y = level_prop->cells_y;
	int z = level_prop->cells_z;
	load_ini_file(level_ini_fields, buf2, 0);

	// Restore map size to match blocks.
	level_prop->cells_x = x;
	level_prop->cells_y = y;
	level_prop->cells_z = z;
	unlock_shared();

	printf_chat("&SFile loaded");
    }
}

void
cmd_save(UNUSED char * cmd, char * arg)
{
    if (!client_ipv4_localhost)
	printf_chat("&WUsage: /save [Auth] filename");
    else if (!arg || !*arg)
	printf_chat("&WUsage: /save filename");
    else {
	char fixedname[200], buf2[256];
	fix_fname(fixedname, sizeof(fixedname), arg);

	snprintf(buf2, sizeof(buf2), "backup/%s.cw", fixedname);
	save_map_to_file(buf2, 0);
	snprintf(buf2, sizeof(buf2), "backup/%s.ini", fixedname);
	save_ini_file(level_ini_fields, buf2);
	printf_chat("&SFile saved");
    }
    return;
}

int
save_level_in_map(char * levelname)
{
    char buf1[256], buf2[256];
    snprintf(buf1, sizeof(buf1), LEVEL_TMP_NAME, levelname);
    snprintf(buf2, sizeof(buf2), LEVEL_CW_NAME, levelname);
    if (access(buf2, F_OK) == 0 && access(buf2, W_OK) != 0) {
	// map _file_ is write protected; don't replace.
	return 1;
    }

    fprintf(stderr, "Saving %s to map directory\n", levelname);

    lock_shared();
    if (save_map_to_file(buf1, 1) < 0) {
	(void)unlink(buf1);
	fprintf(stderr, "map save of '%s' to '%s' failed\n", levelname, buf1);
	return -1;
    } else if (rename(buf1, buf2) < 0) {
	perror("save rename failed");
	(void)unlink(buf1);
	return -1;
    } else
	level_prop->dirty_save = 0;
    unlock_shared();

    return 0;
}

void
scan_and_save_levels()
{
    char * levelname = 0;
    int this_is_main = 0;

    open_client_list();
    stop_shared();
    stop_block_queue();

    for(int lvid=0; lvid<MAX_USER; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;

	nbtstr_t lv = shdat.client->levels[lvid].level;
	levelname = lv.c;
	this_is_main = (strcmp(levelname, server.main_level) == 0);

	open_level_files(levelname, 2);
	if (!level_prop) continue;

	if (level_prop->readonly) {
	    level_prop->dirty_save = 0;
	    level_prop->dirty_backup = 0;
	}

	if (!level_prop->readonly) {
	    if (level_prop->dirty_save) {
		int rv = save_level_in_map(levelname);
		if (rv == 1) {
		    fprintf(stderr, "Level %s write protected, changes discarded\n", levelname);
		    level_prop->dirty_save = 0;
		    level_prop->dirty_backup = 0;
		    level_prop->readonly = 1;
		}
	    }

	    // If the map is clean and it's time for a backup.
	    if (!level_prop->dirty_save && level_prop->dirty_backup) {
		// Do backup map/* to backup/*
		//copy_level_map_to_backup(levelname);
	    }
	}

	stop_shared();

	lock_client_data();

	int user_count = 0;
	for(int uid=0; uid<MAX_USER; uid++)
	{
	    if (shdat.client->user[uid].active != 1) continue;
	    // NB: Only unload main when the _total_ user count hits zero.
	    if (lvid == shdat.client->user[uid].on_level || this_is_main)
		user_count++;
	}

	if (user_count>0) {
	    unlock_client_data();
	    continue;
	}

	// unload.
	unlink_level(levelname, 0);
	shdat.client->levels[lvid].loaded = 0;
	fprintf(stderr, "Unloaded level %s\n", levelname);

	unlock_client_data();
    }

    stop_client_list();
}
