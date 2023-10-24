
#include <dirent.h>

#include "cmdgoto.h"

/*HELP goto,g,gr,gotorandom H_CMD
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
&T/main [Level]
Set the system main level to "level"
*/

#if INTERFACE
#define UCMD_GOTOMAIN \
    {N"goto", &cmd_goto, CMD_HELPARG}, {N"g", &cmd_goto, CMD_ALIAS}, \
    {N"gotorandom", &cmd_goto, CMD_ALIAS, .nodup=1}, \
    {N"goto-random", &cmd_goto, CMD_ALIAS}, \
    {N"gr", &cmd_goto, CMD_ALIAS}, \
    {N"main", &cmd_main}, {N"void", &cmd_void, CMD_ALIAS}
#endif

void
cmd_goto(char * cmd, char * arg)
{
    char fixedname[MAXLEVELNAMELEN*4], buf2[256], levelname[MAXLEVELNAMELEN+1];
    if (!arg && strcmp(cmd, "gotorandom") != 0) { cmd_help(0,"goto"); return; }
    if (!arg) arg = "";

    if (strcasecmp(arg, "-random") == 0) {cmd = "/gotorandom"; arg = "";}

    if (level_loader_pid > 0) {
	printf_chat("Cannot use /%s, already joining a map.", cmd);
	return;
    }

    if (player_lockout>0 || level_processor_pid > 0) {
	printf_chat("Cannot use /%s, current map being processed.", cmd);
	return;
    }

    while (*arg == ' ') arg++;
    int l = strlen(arg);
    // A quoted name is used exactly.
    if (l>1 && arg[0] == '"' && arg[l-1] == '"') {
	arg[l-1] = 0;
	arg++;
	fix_fname(fixedname, sizeof(fixedname), arg);
	saprintf(buf2, LEVEL_CW_NAME, fixedname);
	if (access(buf2, F_OK) != 0) {
	    printf_chat("&SLevel \"%s\" does not exist", arg);
	    return;
	}
    } else if (strcmp(cmd, "gotorandom") == 0) {
	*fixedname = 0;
	choose_random_level(fixedname, sizeof(fixedname));
	if (!*fixedname) {
	    printf_chat("&WFailed to find another level");
	    return;
	}
    } else if (strcasecmp(arg, "+") == 0 || strcasecmp(arg, "-") == 0) {
	char userlevel[256];
	saprintf(userlevel, "%.60s+", user_id);
	fix_fname(fixedname, sizeof(fixedname), userlevel);
	saprintf(buf2, LEVEL_CW_NAME, fixedname);
	if (access(buf2, F_OK) != 0) {
	    printf_chat("&SYour Level \"%s\" does not exist, use /newlvl to created it", userlevel);
	    return;
	}

    } else {
	fix_fname(fixedname, sizeof(fixedname), arg);

	saprintf(buf2, LEVEL_CW_NAME, fixedname);
	if (access(buf2, F_OK) != 0) {
	    char * newfixed = find_file_match(fixedname, arg);

	    if (newfixed != 0) {
		saprintf(fixedname, "%s", newfixed);
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

    if (current_level_backup_id == 0 && strcmp(levelname, current_level_name) == 0) {
	printf_chat("&SYou're already on &7%s", current_level_name);
	return;
    }

    if (strcmp(levelname, main_level()) == 0)
	cmd_main(0,0);
    else
	direct_teleport(levelname, 0, 0);
}

void
cmd_main(char * UNUSED(cmd), char * arg)
{
    if (arg && *arg) {
        if (!perm_is_admin()) {
            printf_chat("&WPermission denied, need to be admin to update system variables");
            return;
        }

	if (setvar("server", "main", arg))
	    printf_chat("Set main level to &T%s", arg);
	return;
    }

    if (current_level_backup_id == 0 && strcmp(main_level(), current_level_name) == 0) {
	printf_chat("&SYou're already on &7%s", current_level_name);
	return;
    }

    open_main_level();

    if (level_prop) {
	printf_chat("@%s&S went to %s", player_list_name.c, main_level());
    } else {
	printf_chat("@%s&S was sucked into the void.", player_list_name.c);
        printf_chat("&WMain level failed to load, you are nowhere.");
    }

    read_only_message();
}

/* This only finds ASCII case insensitive, not CP437. This is probably fine.
 */
#define NMATCH 20
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

    int l = 0, use_count = 0;
    for(int i = 0; i<NMATCH && i<matchcount; i++) {
	int n = strlen(match[i]) + 3;
	if (l>0 && l+n > 60) break;
	l += n; // fixed names may be longer, so ok.
	use_count++;
    }

    if (matchcount > use_count)
	printf_chat("&S%d levels match \"%s\" including ...", matchcount, levelname);
    else
	printf_chat("&S%d levels match \"%s\"", matchcount, levelname);

    char * line = calloc(l, 1);
    for(int i = 0; i<NMATCH && i<use_count; i++) {
	if (i) strcat(line, ", ");
	char lvlname[MAXLEVELNAMELEN+1];
	unfix_fname(lvlname, sizeof(lvlname), match[i]);
	strcat(line, lvlname);
    }
    printf_chat("&S%s", line);
    free(line);

    return 0;
}

static time_t maplist_time = 0;
static char ** maplist = 0;
static int maplist_sz = 0, maplist_cnt = 0;

void
choose_random_level(char * fixedname, int name_len)
{
    // If they stay on a level for more than 5 minutes, refresh the list.
    if (!maplist || maplist_time + 300 < time(0)) {
	if (maplist) {
	    for(int i = 0; i<maplist_cnt; i++)
		if (maplist[i]) free(maplist[i]);
	    free(maplist);
	    maplist = 0;
	    maplist_sz = maplist_cnt = 0;
	}

	struct dirent *entry;

	DIR *directory = opendir(LEVEL_MAP_DIR_NAME);

	if (!directory) {
	    printf_chat("#No maps found, not even %s is saved yet‼", main_level());
	    return;
	}

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
    }

    maplist_time = time(0);

    *fixedname = 0;
    if (maplist_cnt>0) {
        int which = bounded_random(maplist_cnt);
	snprintf(fixedname, name_len, "%s", maplist[which]);

	// Remove this one from the list, keep the rest for next time.
	free(maplist[which]);
	maplist[which] = 0;
	for(int i = which; i<maplist_cnt-1; i++)
	    maplist[i] = maplist[i+1];
	maplist[maplist_cnt-1] = 0;
	maplist_cnt--;
    }
    if (maplist_cnt <= 0) {
	free(maplist);
	maplist = 0;
	maplist_sz = maplist_cnt = 0;
    }
    return;
}
