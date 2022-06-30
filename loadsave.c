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

#include "loadsave.h"

/*HELP goto,g,gr H_CMD
&T/goto [levelname]
Switch your current level, level name may be a partial match
to the full name and &T/goto +&S takes you to a personal level.
Use &T/maps&S to list levels.
&T/goto -random &Sor &T/gr
Go to a random level.
*/
/*HELP main H_CMD
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
    {N"goto", &cmd_goto}, \
    {N"g", &cmd_goto, .dup=1}, {N"gr", &cmd_goto, .dup=1}, \
    {N"main", &cmd_main}
#endif

void
cmd_goto(UNUSED char * cmd, char * arg)
{
    char fixedname[MAXLEVELNAMELEN*4], buf2[256], levelname[MAXLEVELNAMELEN+1];
    if (!arg && strcmp(cmd, "gr") != 0) { cmd_help(0,"goto"); return; }
    if (!arg) arg = "";

    while (*arg == ' ') arg++;
    int l = strlen(arg);
    // A quoted name is used exactly.
    if (l>2 && arg[0] == '"' && arg[l-1] == '"') {
	arg[l-1] = 0;
	arg++;
	fix_fname(fixedname, sizeof(fixedname), arg);
	snprintf(buf2, sizeof(buf2), LEVEL_CW_NAME, fixedname);
	if (access(buf2, F_OK) != 0) {
	    printf_chat("&SLevel \"%s\" does not exist", arg);
	    return;
	}
    } else if (strcmp(cmd, "gr") == 0 || strcasecmp(arg, "-random") == 0) {
	choose_random_level(fixedname, sizeof(fixedname));
    } else if (strcasecmp(arg, "+") == 0 || strcasecmp(arg, "-") == 0) {
	char userlevel[256];
	snprintf(userlevel, sizeof(userlevel), "%.60s+", user_id);
	fix_fname(fixedname, sizeof(fixedname), userlevel);
	snprintf(buf2, sizeof(buf2), LEVEL_CW_NAME, fixedname);
	if (access(buf2, F_OK) != 0) {
	    printf_chat("&SYour Level \"%s\" does not exist, use /newlvl to created it", userlevel);
	    return;
	}

    } else {
	fix_fname(fixedname, sizeof(fixedname), arg);

	snprintf(buf2, sizeof(buf2), LEVEL_CW_NAME, fixedname);
	if (access(buf2, F_OK) != 0) {
	    char * newfixed = find_file_match(fixedname, arg);

	    if (newfixed != 0) {
		snprintf(fixedname, sizeof(fixedname), "%s", newfixed);
		free(newfixed);
	    }
	    else return;
	}
    }

    unfix_fname(levelname, sizeof(levelname), fixedname);
    if (*levelname == 0) {
	fprintf_logfile("Error on map name for \"/goto %s\" file:\"%s\"", arg, fixedname);
	if (*arg && !*fixedname)
	    printf_chat("&SNo levels match \"%s\"", arg);
	else
	    printf_chat("&SCould not load level file \"%s\"", fixedname);
	return;
    }

    if (strcmp(levelname, current_level_name) == 0) {
	printf_chat("&SYou're already on &7%s", current_level_name);
	return;
    }

    stop_shared();

    start_level(levelname, fixedname);
    open_level_files(levelname, fixedname, 0);
    if (!level_prop) {
	printf_chat("&WLevel load failed, returning to main");
	cmd_main("","");
	return;
    }
    send_map_file();
    send_spawn_pkt(255, user_id, level_prop->spawn);

    printf_chat("@&S%s went to &7%s", user_id, levelname);
    read_only_message();
}

void
cmd_main(UNUSED char * cmd, UNUSED char * arg)
{
    if (strcmp(main_level(), current_level_name) == 0) {
	printf_chat("&SYou're already on &7%s", current_level_name);
	return;
    }

    char fixedname[NB_SLEN];
    fix_fname(fixedname, sizeof(fixedname), main_level());

    stop_shared();

    start_level(main_level(), fixedname);
    open_level_files(main_level(), fixedname, 0);
    if (!level_prop)
	fatal("Failed to load main.");
    send_map_file();
    send_spawn_pkt(255, user_id, level_prop->spawn);

    printf_chat("@&S%s went to &7%s", user_id, main_level());

    read_only_message();
}

void
cmd_load(UNUSED char * cmd, char * arg)
{
    if (!client_trusted)
	printf_chat("&WUsage: /load [Auth] filename");
    else if (!arg || !*arg)
	printf_chat("&WUsage: /load filename");
    else {
	char buf[200+8], *s, *d;
	d = buf; *d++ = '_';
	for(s=arg; *s && d<buf+sizeof(buf)-8; s++) {
	    if ((*s>='A' && *s<='Z') || (*s>='a' && *s<='z') ||
		(*s>='0' && *s<='9') || *s=='_')
		*d++ = *s;
	}
	strcpy(d, ".ini");

	lock_shared();
	int rv = load_ini_file(level_ini_fields, buf, 0, 1);
	unlock_shared();

	if (rv == 0) {
	    printf_chat("&SFile loaded");
	    level_prop->dirty_save = 1;
	    level_prop->metadata_generation++;
	    level_prop->last_modified = time(0);
	}
    }
}

void
cmd_save(UNUSED char * cmd, char * arg)
{
    if (!client_trusted)
	printf_chat("&WUsage: /save [Auth] filename");
    else if (!arg || !*arg)
	printf_chat("&WUsage: /save filename");
    else {
	char fixedname[200], buf2[256];
	fix_fname(fixedname, sizeof(fixedname), arg);

	snprintf(buf2, sizeof(buf2), "_%s.ini", fixedname);
	save_ini_file(level_ini_fields, buf2);

	printf_chat("&SConfig saved to %s", buf2);
    }
    return;
}

int
save_level(char * level_fname, char * level_name, int save_bkp)
{
    // Process ...
    // 1) Save map to .tmp
    // 2) Link .cw to .bak
    // 3) Rename .tmp to .cw
    // 4) Rename .bak to backup/...cw
    //
    // If not backup omit (2) and (4) has nothing to ren.

    // Return 0=> ok, 1=> ok, to discard changes, -1=> save didn't work.

    char tmp_fn[256], map_fn[256], bak_fn[256];
    snprintf(tmp_fn, sizeof(tmp_fn), LEVEL_TMP_NAME, level_fname);
    snprintf(map_fn, sizeof(map_fn), LEVEL_CW_NAME, level_fname);
    snprintf(bak_fn, sizeof(bak_fn), LEVEL_BAK_NAME, level_fname);
    if (access(map_fn, F_OK) == 0 && access(map_fn, W_OK) != 0) {
	// map _file_ is write protected; don't replace.
	printlog("Discarding changes to %s -- write protected.", level_fname);
	level_prop->dirty_save = 0;
	level_prop->readonly = 1;
	return 0;
    }

    fprintf_logfile("Saving \"%s\" to map directory%s",
	level_name, save_bkp?" with backup of previous":"");

    time_t backup_sav = level_prop->last_backup;
    if (save_bkp) {
	// We're gonna do the backup.
	level_prop->last_backup = time(0);
    }

    lock_shared();
    if (save_map_to_file(tmp_fn, 1) < 0) {
	int e = errno;
	(void) unlink(tmp_fn);
	errno = e;
	level_prop->last_backup = backup_sav;

	fprintf_logfile("map save of '%s' to '%s' failed", level_name, tmp_fn);
	return -1;
    }

    if (save_bkp) {
	if (access(map_fn, F_OK) == 0) {
	    // Do anything else on backup failure?
	    if (link(map_fn, bak_fn) < 0)
		level_prop->last_backup = backup_sav;
	}
    }

    if (rename(tmp_fn, map_fn) < 0) {
	perror("save rename failed");
	int e = errno;
	(void) unlink(tmp_fn);
	errno = e;
	return -1;
    }

    level_prop->dirty_save = 0;
    unlock_shared();

    if (access(bak_fn, F_OK) == 0) {
	char hst_fn[256];
	next_backup_filename(hst_fn, sizeof(hst_fn), level_fname);

	// rename or copy/del bak_fn to hst_fn
	if (rename(bak_fn, hst_fn) < 0) {
	    int txok = 1;
	    if (errno != EXDEV) {
		perror("Rename of bak file to history failed");
		txok = 0;
	    } else {
		snprintf(tmp_fn, sizeof(tmp_fn), "%s/%s.tmp", LEVEL_BACKUP_DIR_NAME, level_fname);

		FILE *ifd, *ofd;
		ifd = fopen(bak_fn, "r");
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

		if (txok && rename(tmp_fn, hst_fn) < 0) {
		    txok = 0;
		}
		if (!txok) {
		    perror("Backup copy and rename failed");
		    int e = errno;
		    (void) unlink(tmp_fn);
		    errno = e;
		} else
		    (void) unlink(bak_fn);
	    }

	    if (txok) {
		fprintf_logfile("Saved backup of \"%s\" to %s", level_name, hst_fn);
	    } else {
		printf_chat("@&SSaving of backup for level \"%s\" failed", level_name);
	    }
	}
    } else
	level_prop->last_backup = backup_sav;

    return 0;
}

void
next_backup_filename(char * bk_name, int bk_len, char * fixedname)
{
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
		while(*s>='0' && *s<='9') { v=v*10+(*s-'0'); s++; }
		if (v>=backup_id && strcmp(s, ".cw") == 0) {
		    backup_id = v+1;
		}
	    }
	}
	closedir(directory);
    }

    snprintf(bk_name, bk_len, LEVEL_BACKUP_NAME, fixedname, backup_id);
}

void
scan_and_save_levels(int unlink_only)
{
    char * level_name = 0;
    int this_is_main = 0;
    int loaded_levels = 0;

    open_client_list();
    if (!shdat.client) return;

    stop_shared();
    stop_block_queue();

    *current_level_name = 0;
    *current_level_fname = 0;

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	loaded_levels++;

	nbtstr_t lv = shdat.client->levels[lvid].level;
	level_name = lv.c;
	this_is_main = (strcmp(level_name, main_level()) == 0);

	char fixedname[MAXLEVELNAMELEN*4];
	fix_fname(fixedname, sizeof(fixedname), level_name);
	open_level_files(level_name, fixedname, 2);
	if (!level_prop) continue;

	if (level_prop->readonly)
	    level_prop->dirty_save = 0;

	if (!level_prop->readonly && !unlink_only) {
	    if (level_prop->dirty_save) {

		// Time to backup ?
		time_t now = time(0);
		int do_bkp = (now - server->backup_interval >= level_prop->last_backup);
		int rv = save_level(fixedname, level_name, do_bkp);

		if (rv < 0)
		    printf_chat("@&WSave of level \"%s\" failed", level_name);
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

	if (user_count == 0) {
	    if (shdat.client->levels[lvid].loaded) {
		// unload.
		unlink_level(fixedname, 0);
		shdat.client->levels[lvid].loaded = 0;
		fprintf_logfile("Unloaded level files %s", level_name);
	    }
	    loaded_levels--;
	}

	unlock_client_data();
    }

    server->loaded_levels = loaded_levels;
    stop_client_list();
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
		// Looks like a match, will it load ?
		char lvlname[MAXLEVELNAMELEN+1];
		*lvlname = 0;
		unfix_fname(lvlname, sizeof(lvlname), nbuf);
		if (*lvlname == 0) continue;

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
	fprintf_logfile("No level file found to match \"%s\"", levelname);
	printf_chat("&SNo levels match \"%s\"", levelname);
	return 0;
    }
    if (matchcount > NMATCH)
	printf_chat("&S%d levels match \"%s\" including ...", matchcount, levelname);
    else
	printf_chat("&S%d levels match \"%s\"", matchcount, levelname);

    int l = 0;
    for(int i = 0; i<NMATCH && i<matchcount; i++)
	l += strlen(match[i]) + 3; // fixed names are longer, so ok.
    char * line = calloc(l, 1);
    for(int i = 0; i<NMATCH && i<matchcount; i++) {
	if (i) strcat(line, ", ");
	char lvlname[MAXLEVELNAMELEN+1];
	unfix_fname(lvlname, sizeof(lvlname), match[i]);
	strcat(line, lvlname);
    }
    printf_chat("&S%s", line);
    free(line);

    return 0;
}

void
choose_random_level(char * fixedname, int name_len)
{
    struct dirent *entry;

    DIR *directory = opendir(LEVEL_MAP_DIR_NAME);

    if (!directory) {
        printf_chat("#No maps found... WTF where is %s‼", main_level());
        return;
    }

    char ** maplist = 0;
    int maplist_sz = 0, maplist_cnt = 0;

    while( (entry=readdir(directory)) )
    {
#if defined(_DIRENT_HAVE_D_TYPE) && defined(DT_REG) && defined(DT_UNKNOWN)
	if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN)
	    continue;
#endif
	int l = strlen(entry->d_name);
	if (l<=3 || strcmp(entry->d_name+l-3, ".cw") != 0) continue;

	char nbuf[MAXLEVELNAMELEN*4];
	char nbuf2[MAXLEVELNAMELEN+1];
	l -= 3;
	if (l>sizeof(nbuf)-2) continue;
	memcpy(nbuf, entry->d_name, l);
	nbuf[l] = 0;
	unfix_fname(nbuf2, sizeof(nbuf2), nbuf);
	if (*nbuf2 == 0) continue;
	l = strlen(nbuf2);
	if (l>MAXLEVELNAMELEN) continue;

	if (maplist_cnt >= maplist_sz) {
	    if (maplist_sz==0) maplist_sz = 32;
	    maplist = realloc(maplist, (maplist_sz *= 2)*sizeof*maplist);
	}
	if (strcmp(nbuf2, current_level_name) != 0)
	    maplist[maplist_cnt++] = strdup(nbuf);
    }
    closedir(directory);

    *fixedname = 0;
    if (maplist_cnt>0) {
#ifdef PCG32_INITIALIZER
        int which = pcg32_boundedrand(maplist_cnt);
#else
	int which = random() % maplist_cnt;
#endif
	snprintf(fixedname, name_len, "%s", maplist[which]);
    }

    for(int i = 0; i<maplist_cnt; i++)
	free(maplist[i]);
    free(maplist);
    return;
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

