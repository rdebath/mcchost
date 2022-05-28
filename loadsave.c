#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>

#include "loadsave.h"
#include "inline.h"

/*HELP goto
&T/goto [levelname]
Switch your current level
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

void
cmd_goto(UNUSED char * cmd, char * arg)
{
    char fixedname[NB_SLEN], buf2[256], levelname[MAXLEVELNAMELEN+1];
    if (!arg) { cmd_help(0,"goto"); return; }

    fix_fname(fixedname, sizeof(fixedname), arg);

    snprintf(buf2, sizeof(buf2), LEVEL_CW_NAME, fixedname);
    if (access(buf2, F_OK) != 0) {
	char * newfixed = find_file_match(fixedname, arg);

	if (newfixed == 0) return;
	if (0) {
	    // TODO ??
	    snprintf(buf2, sizeof(buf2), "map/%s.ini", fixedname);
	    if (access(buf2, F_OK) != 0) {
		printf_chat("&SNo levels match \"%s\"", arg);
		return;
	    }
	}
	snprintf(fixedname, sizeof(fixedname), "%s", newfixed);
	free(newfixed);
    }

    unfix_fname(levelname, sizeof(levelname), fixedname);
    if (*levelname == 0) {
	printf_chat("&SNo levels match \"%s\"", arg);
	return;
    }

    if (strcmp(levelname, current_level_name) == 0) {
	printf_chat("&SYou're already on &7%s", current_level_name);
	return;
    }

    // printf_chat("@&S%s left &7%s", user_id, current_level_name);

    stop_shared();

    start_level(levelname, fixedname);
    open_level_files(fixedname, 0);
    if (!level_prop) {
	printf_chat("&WLevel load failed, returning to main");
	cmd_main("","");
	return;
    }
    send_map_file();
    send_spawn_pkt(255, user_id, level_prop->spawn);

    printf_chat("@&S%s went to &7%s", user_id, levelname);
    if (level_prop->readonly)
	printf_chat("&WLoaded read only map, changes will be discarded");
}

void
cmd_main(UNUSED char * cmd, UNUSED char * arg)
{
    if (strcmp(server.main_level, current_level_name) == 0) {
	printf_chat("&SYou're already on &7%s", current_level_name);
	return;
    }

    char fixedname[NB_SLEN];
    fix_fname(fixedname, sizeof(fixedname), server.main_level);

    stop_shared();

    start_level(server.main_level, fixedname);
    open_level_files(fixedname, 0);
    if (!level_prop)
	fatal("Failed to load main.");
    send_map_file();
    send_spawn_pkt(255, user_id, level_prop->spawn);

    printf_chat("@&S%s went to &7%s", user_id, server.main_level);
}

void
cmd_load(UNUSED char * cmd, char * arg)
{
    if (!client_ipv4_localhost)
	printf_chat("&WUsage: /load [Auth] filename");
    else if (!arg || !*arg)
	printf_chat("&WUsage: /load filename");
    else {
	char buf[200+8], *s, *d;
	for(s=arg, d=buf; *s && d<buf+sizeof(buf)-8; s++) {
	    if ((*s>='A' && *s<='Z') || (*s>='a' && *s<='z') ||
		(*s>='0' && *s<='9') || *s=='_')
		*d++ = *s;
	}
	strcpy(d, ".ini");

	lock_shared();
	int x = level_prop->cells_x;
	int y = level_prop->cells_y;
	int z = level_prop->cells_z;

	int rv = load_ini_file(level_ini_fields, buf, 0);

	// Restore map size to match blocks.
	level_prop->cells_x = x;
	level_prop->cells_y = y;
	level_prop->cells_z = z;
	unlock_shared();

	if (rv == 0)
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
save_level_in_map(char * level_fname)
{
    char buf1[256], buf2[256];
    snprintf(buf1, sizeof(buf1), LEVEL_TMP_NAME, level_fname);
    snprintf(buf2, sizeof(buf2), LEVEL_CW_NAME, level_fname);
    if (access(buf2, F_OK) == 0 && access(buf2, W_OK) != 0) {
	// map _file_ is write protected; don't replace.
	return 1;
    }

    fprintf(stderr, "Saving %s to map directory\n", level_fname);

    lock_shared();
    if (save_map_to_file(buf1, 1) < 0) {
	(void)unlink(buf1);
	fprintf(stderr, "map save of '%s' to '%s' failed\n", level_fname, buf1);
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
scan_and_save_levels(int unlink_only)
{
    char * level_name = 0;
    int this_is_main = 0;

    open_client_list();
    stop_shared();
    stop_block_queue();

    *current_level_name = 0;
    *current_level_fname = 0;

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;

	nbtstr_t lv = shdat.client->levels[lvid].level;
	level_name = lv.c;
	this_is_main = (strcmp(level_name, server.main_level) == 0);

	char fixedname[MAXLEVELNAMELEN*4];
	fix_fname(fixedname, sizeof(fixedname), level_name);
	open_level_files(fixedname, 2);
	if (!level_prop) continue;

	if (level_prop->readonly) {
	    level_prop->dirty_save = 0;
	    level_prop->dirty_backup = 0;
	}

	if (!level_prop->readonly && !unlink_only) {
	    if (level_prop->dirty_save) {
		try_backup(fixedname);

		int rv = save_level_in_map(fixedname);
		if (rv == 1) {
		    fprintf(stderr, "Level %s write protected, changes discarded\n", fixedname);
		    level_prop->dirty_save = 0;
		    level_prop->dirty_backup = 0;
		    level_prop->readonly = 1;
		} else if (rv == 0)
		    level_prop->dirty_backup = 1;
	    }
	}

	int level_dirty = level_prop->dirty_save;

	stop_shared();

	if (level_dirty) continue;

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
	unlink_level(fixedname, 0);
	shdat.client->levels[lvid].loaded = 0;
	fprintf(stderr, "Unloaded level files %s\n", fixedname);

	unlock_client_data();
    }

    stop_client_list();
}

void
try_backup(char * fixedname)
{
    char filename[256];
    snprintf(filename, sizeof(filename), LEVEL_PREV_NAME, fixedname);

    struct stat info;

    // Existing file in backup directory
    if (stat(filename, &info) == 0)
    {
	// If it's old
	time_t now = time(0);
	if (now-server.backup_interval < info.st_mtime) return;
	int backup_id = 1;

	// Find the next unused number
	struct dirent *entry;
	DIR *directory = opendir(LEVEL_BACKUP_DIR_NAME);
	if (directory) {
	    int l = strlen(fixedname);
	    while( (entry=readdir(directory)) )
	    {
		if (strncmp(entry->d_name, fixedname, l) == 0
		    && entry->d_name[l] == '.') {
		    char * s = entry->d_name+l+1;
		    int v = 0;
		    while(*s>='0' && *s<='9') { s++; v=v*10+(*s-'0'); }
		    if (v>backup_id && strcmp(s, ".cw") == 0) {
			backup_id = v+1;
		    }
		}
	    }
	    closedir(directory);
	}

	// Rename it to there
	char backupname[256];
	sprintf(backupname, LEVEL_BACKUP_NAME, fixedname, backup_id);
	if (rename(filename, backupname) < 0) {
	    fprintf(stderr, "Error on backup of %s to %s\n", filename, backupname);
	    return;
	}
    }

    if (access(filename, F_OK) != 0) {

	// No file in the backup dir, copy the saved one.
	char cwfilename[256];
	snprintf(cwfilename, sizeof(cwfilename), LEVEL_CW_NAME, fixedname);

	if (access(cwfilename, F_OK) == 0) {
	    if (rename(cwfilename, filename) < 0) {
		fprintf(stderr, "Error on rename of %s to %s\n", cwfilename, filename);
		return;
	    }
	}
    }
}

/* This only finds ASCII case insensitive, not CP437. This is probably fine.
 */
#define NMATCH 4
char *
find_file_match(char * fixedname, char * levelname)
{
    struct dirent *entry;
    char * exactmatch = 0;
    char * match[NMATCH];
    int matchcount = 0, need_exact_case = 0;

    DIR *directory = opendir(LEVEL_MAP_DIR_NAME);
    if (directory) {
	while( (entry=readdir(directory)) )
	{
	    char nbuf[MAXLEVELNAMELEN*4];
	    int l = strlen(entry->d_name);
	    if (l<3 || strcmp(entry->d_name+l-3, ".cw") != 0)
		continue;
	    if (l>sizeof(nbuf)-1) continue;
	    l -= 3;
	    memcpy(nbuf, entry->d_name, l);
            nbuf[l] = 0;

	    // "Exact" matches, maybe case insensitive.
	    if (strcasecmp(nbuf, fixedname) == 0) {
		if (exactmatch == 0 && need_exact_case == 0) {
		    // Nothing before accept CI
		    exactmatch = strdup(nbuf);
		    if (strcmp(nbuf, fixedname) == 0)
			need_exact_case = 1; // Which we have.
		} else if (exactmatch != 0 && need_exact_case == 0) {
		    // Had a CI now got another, need CS
		    need_exact_case = 1;
		    if(exactmatch) free(exactmatch);
		    exactmatch = 0;
		    // Which this could be.
		    if (strcmp(nbuf, fixedname) == 0)
			exactmatch = strdup(nbuf);
		} else if (exactmatch == 0 && need_exact_case != 0) {
		    // Found a needed CS
		    if (strcmp(nbuf, fixedname) == 0)
			exactmatch = strdup(nbuf);
		}
	    }

	    if (my_strcasestr(nbuf, fixedname) != 0)
	    {
		if (matchcount < NMATCH)
		    match[matchcount] = strdup(nbuf);
		matchcount++;
	    }
	}
	closedir(directory);
    }
    if (exactmatch || matchcount == 1) {
	char * p = exactmatch;
	if (!exactmatch) {
	    p = match[0];
	    match[0] = 0;
	}
	for(int i = 0; i<matchcount && i<NMATCH; i++)
	    if (match[i])
		free(match[i]);
	return p;
    }
    if (matchcount == 0) {
	printf_chat("&SNo levels match \"%s\"", levelname);
	return 0;
    }
    if (matchcount > NMATCH)
	printf_chat("&S%d levels match \"%s\" including ...", matchcount, levelname);
    else
	printf_chat("&S%d levels match \"%s\"", matchcount, levelname);

    int l = 0;
    for(int i = 0; i<NMATCH && i<matchcount; i++)
	l += strlen(match[i]) + 3;
    char * line = calloc(l, 1);
    for(int i = 0; i<NMATCH && i<matchcount; i++) {
	if (i) strcat(line, ", ");
	char lvlname[MAXLEVELNAMELEN+1];
	unfix_fname(lvlname, sizeof(lvlname), match[i]);
	strcat(line, match[i]);
    }
    printf_chat("&S%s", line);
    free(line);

    return 0;
}

char *
my_strcasestr(char *haystack, char *needle)
{
    int nlen = strlen(needle);
    int slen = strlen(haystack) - nlen + 1;
    int i;

    for (i = 0; i < slen; i++) {
	int j;
	for (j = 0; j < nlen; j++) {
	    if (toupper((unsigned char)haystack[i+j]) != toupper((unsigned char)needle[j]))
		goto break_continue;
	}
	return haystack + i;
    break_continue: ;
    }
    return 0;
}

