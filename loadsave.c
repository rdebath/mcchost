#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "loadsave.h"

/*TODO: HELP load
&T/load [name]
*/
/*TODO: HELP save
&T/save [name]
*/

#if INTERFACE
#define CMD_LOADSAVE  {N"load", &cmd_load}, {N"save", &cmd_save}
#endif

static inline int E(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }

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

void
save_level_in_map(char * levelname)
{
    char buf1[256], buf2[256];
    snprintf(buf1, sizeof(buf1), "map/%.200s.tmp", levelname);
    snprintf(buf2, sizeof(buf2), "map/%.200s.cw", levelname);

    lock_shared();
    if (save_map_to_file(buf1, 1) < 0) {
	(void)unlink(buf1);
	fprintf(stderr, "map save of '%s' to '%s' failed\n", levelname, buf1);
    } else if (rename(buf1, buf2) < 0) {
	perror("save rename failed");
	(void)unlink(buf1);
    } else
	level_prop->dirty_save = 0;
    unlock_shared();
}

void
scan_and_save_levels()
{
    char * levelname = 0;

    open_client_list();
    stop_shared();
    stop_block_queue();
    level_name = 0;

    for(int lvid=0; lvid<MAX_USER; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;

	nbtstr_t lv = shdat.client->levels[lvid].level;
	levelname = lv.c;

	open_level_files(levelname, 2);
	if (!level_prop) continue;

	if (level_prop->dirty_save) {
	    fprintf(stderr, "Saving %s to maps\n", levelname);
	    save_level_in_map(levelname);
	}

	if (!level_prop->dirty_save && level_prop->dirty_backup) {
	    // Do backup map/* to backup/*
	}

	stop_shared();
	stop_block_queue();
	level_name = 0;

	lock_client_data();

	int user_count = 0;
	for(int uid=0; uid<MAX_USER; uid++)
	{
	    if (shdat.client->user[uid].active != 1) continue;
	    if (lvid == shdat.client->user[uid].on_level)
		user_count++;
	}

	if (user_count>0) {
	    unlock_client_data();
	    continue;
	}

	// unload.
	fprintf(stderr, "Unloading %s\n", levelname);
	unlink_level(levelname, 0);
	shdat.client->levels[lvid].loaded = 0;

	unlock_client_data();
    }

    stop_client_list();
}
