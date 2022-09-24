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

#include "cmdgoto.h"

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

#if INTERFACE
#define CMD_GOTOMAIN \
    {N"goto", &cmd_goto}, \
    {N"g", &cmd_goto, .dup=1}, {N"gr", &cmd_goto, .dup=1, .nodup=1}, \
    {N"main", &cmd_main}, {N"void", &cmd_void, .dup=1}
#endif

void
cmd_goto(char * cmd, char * arg)
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
	saprintf(buf2, LEVEL_CW_NAME, fixedname);
	if (access(buf2, F_OK) != 0) {
	    printf_chat("&SLevel \"%s\" does not exist", arg);
	    return;
	}
    } else if (strcmp(cmd, "gr") == 0 || strcasecmp(arg, "-random") == 0) {
	choose_random_level(fixedname, sizeof(fixedname));
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

    direct_teleport(levelname, 0, 0);
}

void
cmd_main(char * UNUSED(cmd), char * UNUSED(arg))
{
    if (current_level_backup_id == 0 && strcmp(main_level(), current_level_name) == 0) {
	printf_chat("&SYou're already on &7%s", current_level_name);
	return;
    }

    char fixedname[NB_SLEN];
    fix_fname(fixedname, sizeof(fixedname), main_level());

    stop_shared();

    start_level(main_level(), fixedname, 0);
    open_level_files(main_level(), 0, fixedname, 0);

    if (!level_prop) {
        start_level(main_level(), fixedname, -1);
        if (level_prop) {
            level_prop->readonly = 1;
            level_prop->disallowchange = 0;
        }
    }

    if (level_prop) player_posn = level_prop->spawn;
    send_map_file();

    if (level_prop) {
	send_addentity_pkt(255, user_id, user_id, level_prop->spawn);
	printf_chat("@&S%s went to &7%s", user_id, main_level());
    } else {
	printf_chat("@&S%s was sucked into the void.", user_id);
        printf_chat("&WMain level failed to load, you are nowhere.");
    }

    read_only_message();
}

void
cmd_void(char * UNUSED(cmd), char * UNUSED(arg))
{
    char * voidname = "<void>";

    char fixedname[NB_SLEN];
    fix_fname(fixedname, sizeof(fixedname), voidname);
    stop_shared();
    start_level(voidname, fixedname, -1);
    // open_level_files(voidname, 0, fixedname, 0);
    if (level_prop) {
	level_prop->readonly = 1;
	level_prop->disallowchange = 0;
    }
    if (level_prop) player_posn = level_prop->spawn;
    send_map_file();

    printf_chat("@&S%s was sucked into the void.", user_id);
    printf_chat("&SYou jumped into the void");
}

int
direct_teleport(char *level, int backup_id, xyzhv_t *npos)
{
    char fixedname[MAXLEVELNAMELEN*4], buf2[256], levelname[MAXLEVELNAMELEN+1];

    fix_fname(fixedname, sizeof(fixedname), level);

    if (backup_id == 0) {
	if (!check_level(level, fixedname)) {
	    unfix_fname(levelname, sizeof(levelname), fixedname);
	    printf_chat("&WLevel &S%s&W is not available.", levelname);
	    return 0;
	}

        saprintf(buf2, LEVEL_CW_NAME, fixedname);
        if (access(buf2, F_OK) != 0) {
            printf_chat("&SLevel \"%s\" does not exist", level);
            return 0;
        }

    } else if (backup_id > 0) {

	saprintf(buf2, LEVEL_BACKUP_NAME, fixedname, backup_id);
	if (access(buf2, F_OK) != 0){
	    fprintf_logfile("Backup file \"%s\" does not exist.", buf2);
	    printf_chat("&SBackup %d for level %s does not exist", backup_id, level);
	    return 0;
	}
    } else
	return 0;

    unfix_fname(levelname, sizeof(levelname), fixedname);
    if (*levelname == 0) {
	fprintf_logfile("Error on map name in direct_teleport \"%s\" file:\"%s\"", level, fixedname);
	if (*level && !*fixedname)
	    printf_chat("&SNo levels match \"%s\"", level);
	else
	    printf_chat("&SCould not load level file \"%s\"", fixedname);
	return 0;
    }


    if (backup_id) {
	char fixedname2[MAXLEVELNAMELEN*4];
	strcpy(fixedname2, fixedname);
	saprintf(fixedname, "%s.%d", fixedname2, backup_id);
    }

    stop_shared();

    start_level(levelname, fixedname, backup_id);
    open_level_files(levelname, backup_id, fixedname, 0);
    if (!level_prop) {
        printf_chat("&WLevel load failed, returning to main");
        cmd_main(0,0);
        return 0;
    }

    if (backup_id) {
	level_prop->readonly = 1;
	level_prop->disallowchange = 0;
    }

    send_map_file();
    send_addentity_pkt(255, user_id, user_id, level_prop->spawn);

    if (backup_id)
	printf_chat("@&S%s went to museum %d of &7%s", user_id, backup_id, levelname);
    else {
	printf_chat("@&S%s went to &7%s", user_id, levelname);
	read_only_message();
    }

    if (npos) {
	send_posn_pkt(255, &player_posn, *npos);
	player_posn = *npos;
    }

    return 1;
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
        printf_chat("#No maps found, not even %s is saved yet‼", main_level());
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
        int which = bounded_random(maplist_cnt);
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

