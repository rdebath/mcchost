#include <sys/stat.h>
#include <dirent.h>

#include "loadsave.h"

filelock_t level_save_lock[1] = {{.name = SAVE_LOCK_NAME}};

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

    // Return 0=> ok, -1=> save didn't work.

    char tmp_fn[256], map_fn[256], bak_fn[256];
    int cw_ok = 1;
    saprintf(tmp_fn, LEVEL_TMP_NAME, level_fname);
    saprintf(map_fn, LEVEL_CW_NAME, level_fname);
    saprintf(bak_fn, LEVEL_BAK_NAME, level_fname);
    if ((cw_ok = (access(map_fn, F_OK) == 0)) && access(map_fn, W_OK) != 0) {
	// map _file_ is write protected; don't replace.
	printlog("Discarding changes to %s -- write protected.", level_fname);
	level_prop->dirty_save = 0;
	level_prop->readonly = 1;
	return 0;
    }

    if (save_bkp == 2)
	fprintf_logfile("Saving \"%s\" as backup only to delete level.", level_name);
    else
	fprintf_logfile("Saving \"%s\" to map directory%s",
	    level_name, save_bkp && cw_ok?" with backup of previous":"");

    lock_fn(level_save_lock);	// Only one save at a time.

    int backup_ok = 1;

    if (save_map_to_file(tmp_fn, 1) < 0) {
	int e = errno;
	(void) unlink(tmp_fn);
	unlock_fn(level_save_lock);
	errno = e;

	fprintf_logfile("map save of '%s' to '%s' failed", level_name, tmp_fn);
	return -1;
    }

    if (save_bkp && save_bkp != 2) {
	if (access(map_fn, F_OK) == 0) {
	    // Do anything else on backup failure?
	    if (link(map_fn, bak_fn) < 0) backup_ok = 0;
	}
    }

    if (rename(tmp_fn, map_fn) < 0) {
	perror("save rename failed");
	int e = errno;
	(void) unlink(tmp_fn);
	unlock_fn(level_save_lock);
	errno = e;
	return -1;
    }

    if (save_bkp == 2) {
	if (rename(map_fn, bak_fn) < 0) {
	    perror("backup rename failed");
	    int e = errno;
	    unlock_fn(level_save_lock);
	    errno = e;
	    return -1;
	}
    }
    level_prop->dirty_save = 0;
    unlock_fn(level_save_lock);

    if (access(bak_fn, F_OK) == 0) {
	if (backup_ok)
	    level_prop->last_backup = time(0);

	move_file_to_backups(bak_fn, level_fname, level_name);
    }

    if (save_bkp == 2) {
	char buf3[256];
	saprintf(buf3, LEVEL_INI_NAME, level_fname);
	(void)unlink(buf3); // Remove additional data file.
    } else
	save_level_ini(level_fname); // Incl last modified time
    return 0;
}

void
move_file_to_backups(char * bak_fn, char * level_fname, char * level_name)
{
    if (access(bak_fn, F_OK) != 0) return;

    char hst_fn[256];
    next_backup_filename(hst_fn, sizeof(hst_fn), level_fname);

    // rename or copy/del bak_fn to hst_fn
    if (rename(bak_fn, hst_fn) < 0) {
	int txok = 1;
	if (errno != EXDEV) {
	    perror("Rename of bak file to history failed");
	    txok = 0;
	} else {
	    char tmp_fn[256];
	    saprintf(tmp_fn, "%s/%s.tmp", LEVEL_BACKUP_DIR_NAME, level_fname);

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
	    fprintf_logfile("Copied backup of \"%s\" to %s", level_name, hst_fn);
	} else {
	    printf_chat("@&SSaving of backup for level \"%s\" failed", level_name);
	}
    }
}

void
next_backup_filename(char * bk_name, int bk_len, char * fixedname)
{
    int backup_id = 0;
    char path[MAXLEVELNAMELEN*4];

    for(int i = 0; i<2 && backup_id == 0; i++) {
	// Find the next unused number
	struct dirent *entry;
	DIR *directory = 0;
	if (i == 0)
	    directory = opendir(LEVEL_BACKUP_DIR_NAME);
	else {
	    snprintf(path, sizeof(path), LEVEL_BACKUP_DIR_NAME "/%s", fixedname);
	    directory = opendir(path);
	}
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
    }
    if (backup_id <= 0) backup_id = 1;

    snprintf(bk_name, bk_len, LEVEL_BACKUP_NAME, fixedname, backup_id);
}

int
scan_and_save_levels(int do_timed_save)
{
    char * level_name = 0;
    int loaded_levels = 0;
    int check_again = 0;
    int trigger_full_run = 0;
    int level_deleted = 0;
    nbtstr_t deleted_level = {0};
    int level_saved = 0;
    nbtstr_t saved_level = {0};
    int level_unloaded = 0;
    nbtstr_t unloaded_level = {0};

    // printlog("scan_and_save_levels(%s)", !do_timed_save?"unlink":"backup");

    open_client_list();
    if (!shdat.client) return 0;

    stop_shared();
    stop_block_queue();
    lock_restart(level_save_lock);

    *current_level_name = 0;

    shdat.client->cleanup_generation = shdat.client->generation;

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	loaded_levels++;

	int user_count = 0;
	set_level_in_use_flag(lvid, &user_count);
	int level_in_use = shdat.client->levels[lvid].in_use;

	if (!do_timed_save) {
	    if (shdat.client->levels[lvid].force_backup)
		trigger_full_run = 1;

	    if (level_in_use && !shdat.client->levels[lvid].force_backup)
		continue;

	    if (shdat.client->levels[lvid].delete_on_unload)
		trigger_full_run = 1;
	}

	if (shdat.client->levels[lvid].backup_id == 0)
	{
	    // Check for saves and backups ... unpacked backups don't have either.
	    // Also check the "NoUnload" flag

	    nbtstr_t lv = shdat.client->levels[lvid].level;
	    level_name = lv.c;

	    char fixedname[MAXLEVELNAMELEN*4];
	    fix_fname(fixedname, sizeof(fixedname), level_name);
	    open_level_files(level_name, 0, 0, fixedname, 1);
	    if (!level_prop)
	    {
		// I can't open the level ... hmmm.
		fprintf_logfile("Failed to open level '%s' for save", level_name);
		ignore_broken_level(lvid, &loaded_levels);
		continue;
	    }

	    shdat.client->levels[lvid].no_unload = level_prop->no_unload;
	    set_level_in_use_flag(lvid, &user_count);
	    level_in_use = shdat.client->levels[lvid].in_use;

	    int force_backup = 0; // And delete
	    if (shdat.client->levels[lvid].delete_on_unload) {
		level_prop->dirty_save = 1;
		force_backup = 1;
	    }

	    int preserve_current = 0;
	    if (shdat.client->levels[lvid].force_backup) {
		shdat.client->levels[lvid].force_backup = 0;
		level_prop->force_save = 1;
		level_prop->dirty_save = 1;
		preserve_current = 1;
	    }

	    if (level_prop->readonly && !level_prop->force_save)
		level_prop->dirty_save = 0;

	    // Block unload unless we are restarting.
	    int no_unload = shdat.client->levels[lvid].in_use &&
		!shdat.client->levels[lvid].force_unload &&
		!restart_on_unload;

	    if (level_prop->dirty_save) {
		int do_save = do_timed_save;
		if (server->no_save_inuse && user_count && no_unload)
		    do_save = 0; // Don't save while players actually on level.

		// Time to backup ?
		time_t now = time(0), last_backup = level_prop->last_backup;
		if (last_backup == 0) last_backup = level_prop->time_created;
		int do_bkp = (now - server->backup_interval >= last_backup);
		if (force_backup) do_bkp = 2; // NB: Saves delete time too.
		if (do_bkp && do_timed_save) do_save = 1;

		if (do_save) {
		    int rv = save_level(fixedname, level_name, do_bkp);
		    if (rv < 0)
			printf_chat("@&WSave of level \"%s\" failed", level_name);
		}
	    }

	    if (preserve_current) {
		char map_fn[256], bak_fn[256];
		saprintf(map_fn, LEVEL_CW_NAME, fixedname);
		saprintf(bak_fn, LEVEL_BAK_NAME, fixedname);
		if (link(map_fn, bak_fn) >= 0) {
		    fprintf_logfile("Saving additional backup of \"%s\"", level_name);
		    move_file_to_backups(bak_fn, fixedname, level_name);
		    level_saved = 1;
		    saved_level = lv;
		}
	    }

	    int level_dirty = level_prop->dirty_save;

	    stop_shared();

	    // Don't unload a map that's still dirty
	    if (level_dirty) continue;

	    // Don't unload if it's turned off
	    if (no_unload) continue;
	} else {
	    if (shdat.client->levels[lvid].force_backup)
		shdat.client->levels[lvid].force_backup = 0;
	}

	// We only want to unlink unused maps
	if (level_in_use) continue;

	lock_fn(system_lock);

	// Recount under lock.
	user_count = 0;
	for(int uid=0; uid<MAX_USER; uid++)
	{
	    if (shdat.client->user[uid].active != 1) continue;
	    if (lvid == shdat.client->user[uid].on_level ||
		lvid == shdat.client->user[uid].summon_level_id)
		user_count++;
	}

	// Don't try to unload a level if there are users on it.
	if (user_count)
	    check_again = 1;
	else {
	    // recheck as someone may have unloaded it.
	    if (shdat.client->levels[lvid].loaded) {
		// unload.
		nbtstr_t lv = shdat.client->levels[lvid].level;
		level_name = lv.c;
		if (shdat.client->levels[lvid].delete_on_unload) {
		    deleted_level = lv;
		    level_deleted = 1;
		} else if (shdat.client->levels[lvid].force_unload) {
		    unloaded_level = lv;
		    level_unloaded = 1;
		}
		char fixedname[MAXLEVELNAMELEN*4];
		int backup_id = shdat.client->levels[lvid].backup_id;
		fix_fname(fixedname, sizeof(fixedname), level_name);
		if (shdat.client->levels[lvid].backup_id>0) {
		    char fixedname2[MAXLEVELNAMELEN*4];
		    strcpy(fixedname2, fixedname);
		    saprintf(fixedname, "%s.%d",
			fixedname2, backup_id);
		}

		if (backup_id>=0) {
		    unlink_level(fixedname, 0);

		    // We known nobody may have the level lock and we have the system
		    // lock, so it's safe to delete the pthread mutex file too.
		    char sharename[256];
		    saprintf(sharename, LEVEL_LOCK_NAME, fixedname);
		    (void)unlink(sharename);
		}

		shdat.client->levels[lvid].loaded = 0;
		if (backup_id>0)
		    fprintf_logfile("Unloaded backup %d of %s", backup_id, level_name);
		else if (backup_id == 0)
		    fprintf_logfile("Unloaded level %s", level_name);
	    }
	    loaded_levels--;
	}

	unlock_fn(system_lock);
    }

    if (server->loaded_levels != loaded_levels) {
	server->loaded_levels = loaded_levels;
	server->pinned_levels = 0;
    }

    if (check_again) shdat.client->generation++;

    if (level_deleted) {
	// Do this outside the system lock.
	level_deleted = 0;
	printf_chat("@Level %s deleted", deleted_level.c);
	stop_chat_queue();
    }

    if (level_saved) {
	// Do this outside the system lock.
	level_saved = 0;
	printf_chat("@Level %s backed up", saved_level.c);
	stop_chat_queue();
    }

    if (level_unloaded) {
	// Do this outside the system lock.
	level_unloaded = 0;
	printf_chat("@Level %s unloaded", unloaded_level.c);
	stop_chat_queue();
    }

    return trigger_full_run;
}

void
save_level_ini(char * level_name)
{
    char fixedname[MAXLEVELNAMELEN*4];
    fix_fname(fixedname, sizeof(fixedname), level_name);
    char ini_file[256];
    saprintf(ini_file, LEVEL_INI_NAME, fixedname);
    save_ini_file(mcc_level_ini_fields, ini_file);
}

LOCAL void
ignore_broken_level(int lvid, int *loaded_levels)
{
    lock_fn(system_lock);

    int user_count = 0;
    for(int uid=0; uid<MAX_USER; uid++)
    {
	if (shdat.client->user[uid].active != 1) continue;
	// NB: Only unload main when the _total_ user count hits zero.
	if (lvid == shdat.client->user[uid].on_level ||
	    lvid == shdat.client->user[uid].summon_level_id)
	    user_count++;
    }

    if (user_count == 0) {
	if (shdat.client->levels[lvid].loaded)
	    shdat.client->levels[lvid].loaded = 0;
	(*loaded_levels)--;
    }

    unlock_fn(system_lock);
}

int
scan_levels()
{
    int loaded_levels = 0, pinned_levels = 0, rv = 0;

    int flg = (shdat.client == 0);
    if (flg) open_client_list();
    if (!shdat.client) return 0;

    if (shdat.client->cleanup_generation != shdat.client->generation) {
	shdat.client->cleanup_generation = shdat.client->generation;
	if (flg) stop_client_list();
	return 1;
    }

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	loaded_levels++;

	if (shdat.client->levels[lvid].force_backup)
	    rv = 1;

	if (!set_level_in_use_flag(lvid,0))
	    rv = 1;

	if (shdat.client->levels[lvid].in_use)
	    pinned_levels++;
    }

    if (server->loaded_levels != loaded_levels)
	server->loaded_levels = loaded_levels;

    if (server->pinned_levels != pinned_levels)
	server->pinned_levels = pinned_levels;

    if (flg) stop_client_list();
    return rv;
}

LOCAL int
set_level_in_use_flag(int lvid, int * users_on_level)
{
    int this_is_main = (
		(shdat.client->levels[lvid].backup_id == 0) &&
		(strcmp(shdat.client->levels[lvid].level.c, main_level()) == 0)
	    );

    int user_count = 0, other_users = 0, level_in_use = 0;
    for(int uid=0; uid<MAX_USER; uid++)
    {
	if (shdat.client->user[uid].active != 1) continue;
	// NB: Only unload main when the _total_ user count hits zero.
	if (lvid == shdat.client->user[uid].on_level ||
	    lvid == shdat.client->user[uid].summon_level_id) {
	    user_count++;
	    level_in_use = 1;
	} else other_users++;
    }
    if (users_on_level) *users_on_level = user_count;
    // Main and other normal levels may be held open.
    if (shdat.client->levels[lvid].backup_id == 0
	&& !restart_on_unload && !term_sig
	&& !shdat.client->levels[lvid].force_unload
	&& !shdat.client->levels[lvid].delete_on_unload)
    {
	if (shdat.client->levels[lvid].no_unload) level_in_use = 1;
	if (this_is_main && !level_in_use) {
	    if (server->no_unload_main) level_in_use = 1;
	    if (other_users>0) level_in_use = 1;
	    user_count += other_users;
	}
    }
    if (shdat.client->levels[lvid].in_use == level_in_use)
	return 1;
    shdat.client->levels[lvid].in_use = level_in_use;
    return 0;
}
