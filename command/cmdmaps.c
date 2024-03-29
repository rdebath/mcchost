#include <dirent.h>

#include "cmdmaps.h"

/*HELP maps,levels,worlds H_CMD
&T/maps
List out the available levels
use &T/maps [number]&S to start at that position
and &T/maps all&S to show all maps.
and &T/maps backup&S to show all backups.
and &T/maps all pattern&S to show maps matching pattern.
and &T/maps backup pattern&S to show backups matching pattern.

If a backup entry has a suffix of &T[*]&S it is a subdirectory
that can be listed by using &T/maps backup directory&S
and visited using &T/museum directory/level 1&S
*/

#if INTERFACE
#define UCMD_MAPS \
    {N"maps", &cmd_maps}, {N"levels", &cmd_maps, CMD_ALIAS}, \
    {N"worlds", &cmd_maps, CMD_ALIAS}, {N"unloaded", &cmd_maps, CMD_ALIAS}

typedef struct maplist_entry_t maplist_entry_t;
struct maplist_entry_t {
    char * name;
    int backup_id;
    int backup_id2;
};

typedef struct maplist_t maplist_t;
struct maplist_t {
    int count;
    int size;
    struct maplist_entry_t *entry;
};
#endif

static int
pmaplistentrycmp(const void *p1, const void *p2)
{
    maplist_entry_t *e1 = (maplist_entry_t *)p1;
    maplist_entry_t *e2 = (maplist_entry_t *)p2;

    int r = strnatcasecmp(e1->name, e2->name);
    if (r) return r;
    /* We may get name duplicates, this stabilises the final order. */
    r = strcmp(e1->name, e2->name);
    if (r) return r;
    /* Then sort by backup_id (int) */
    return e1->backup_id - e2->backup_id;
}


void
cmd_maps(char * UNUSED(cmd), char * arg)
{
    int start = 0, backups = 0;
    char *ar1 = 0, *ar2 = 0;
    int filtered = 0;

    if (!arg || *arg == 0) start = 1;
    else {
	ar1 = strarg(arg); ar2 = strarg_rest();
	if (strcasecmp(ar1, "all") == 0) start = 0;
	else if (strcasecmp(ar1, "backup") == 0) {
	    backups = 1;
	} else if (strcasecmp(ar1, "backups") == 0) {
	    backups = 2;
	    if (ar2 == 0) ar2 = current_level_name;
	} else {
	    start = atoi(ar1);
	    if (start <= 0) {
		if (ar2 != 0) {
		    printf_chat("&eFirst arg must be either \"all\", \"backup\" or an integer.");
		    return;
		} else
		    ar2 = ar1;
	    }
	}
    }

    maplist_t maps = {0};
    filtered = (ar2 && *ar2 != 0);
    if (!backups) {
	DIR *directory = opendir("map");
	if (directory) {
	    read_maps(&maps, 0, directory, ar2);
	    closedir(directory);
	}
    }
    if (backups) {
	int flg = 0;
	if (ar2 && *ar2) {
	    char sbuf[PATH_MAX];
	    char fixedname_dir[MAXLEVELNAMELEN*4];
	    fix_fname(fixedname_dir, sizeof(fixedname_dir), ar2);
	    saprintf(sbuf, LEVEL_BACKUP_DIR_NAME "/%s", fixedname_dir);
	    DIR *directory = opendir(sbuf);
	    if (directory) {
		read_maps(&maps, 1, directory, 0);
		closedir(directory);
		flg = 1;
	    }
	}
	if (flg == 0) {
	    DIR *directory = opendir(LEVEL_BACKUP_DIR_NAME);
	    if (directory) {
		read_maps(&maps, backups, directory, ar2);
		closedir(directory);
	    }
	}
    }

    qsort(maps.entry, maps.count, sizeof(*maps.entry), pmaplistentrycmp);

    char line_buf[NB_SLEN] = {0};
    int c = 0, end = maps.count;
    if (start == 0) end = maps.count;
    else { start--; end = start + 30; }
    if (end>maps.count) end = maps.count;

    if (!maps.count) {
	if (!filtered)
	    printf_chat("&WNo %s have been saved yet", backups?"backups":"levels");
	else
	    printf_chat("&WNo %s match \"%s\"", backups?"backups":"levels", ar2);
	return;
    }

    if (!backups)
	printf_chat("&SShowing levels %d-%d (out of %d)", start+1,end,maps.count);
    else if (backups == 2)
	printf_chat("&SShowing %d backups for %s", maps.count, ar2);
    else
	printf_chat("&SShowing %d backups", maps.count);
    for(int i = start; i<end; i++) {
	char * s = maps.entry[i].name;
	int l = strlen(s), l0=l;
	char num[sizeof(int)*3*2+16];
	if (maps.entry[i].backup_id == 0)
	    *num = 0;
	else {
	    int m1 = maps.entry[i].backup_id, m2 = m1;

	    while (i+1<end && maps.entry[i+1].backup_id == m2+1) {
		if (strcmp(maps.entry[i].name, maps.entry[i+1].name) != 0)
		    break;
		m2++; i++;
	    }

	    if (m1 == m2) {
		if (m1 == -1)
		    l += sprintf(num, "[*]");
		else if (m1 == 1)
		    *num = 0;
		else
		    l += sprintf(num, "[%d]", maps.entry[i].backup_id);
	    } else if (m1+1 == m2) {
		l += sprintf(num, "[%d,%d]", m1, m2);
	    } else
		l += sprintf(num, "[%d-%d]", m1, m2);
	}

	if (c != 0 && c + 2 + l > NB_SLEN-1-3) {
	    printf_chat("&7%s,", line_buf);
	    c = 0; *line_buf = 0;
	}
	if (c) {
	    strcpy(line_buf+c, ", ");
	    c += 2;
	}
	strcpy(line_buf+c, s);
	if (*num) strcpy(line_buf+c+l0, num);
	c += l;
    }
    if (c)
	printf_chat("&7%s", line_buf);

    for(int i = 0; i<maps.count; i++)
	free(maps.entry[i].name);
    free(maps.entry);
}

void
read_maps(maplist_t * maps, int is_backup, DIR *directory, char * matchstr)
{
    struct dirent *entry;
    int matchstr_len = matchstr?strlen(matchstr):0;

    while( (entry=readdir(directory)) )
    {
	enum { ft_unk, ft_file, ft_dir } ft = ft_unk;
#if defined(_DIRENT_HAVE_D_TYPE) && defined(DT_REG) && defined(DT_UNKNOWN) && defined(DT_DIR)
	if (entry->d_type == DT_DIR) {
	    if (!is_backup) continue;
	    ft = ft_dir;
	} else if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN)
	    continue;
	else
	    ft = ft_file;
#endif
	int l = strlen(entry->d_name);
	if (ft != ft_dir) {
	    if (l<=3 || strcmp(entry->d_name+l-3, ".cw") != 0) continue;
	    l -= 3;
	}

	char nbuf[MAXLEVELNAMELEN*4+sizeof(int)*3+3];
	char nbuf2[MAXLEVELNAMELEN+1];
	int backup_id = 0;

	if (l>sizeof(nbuf)-2) continue;
	memcpy(nbuf, entry->d_name, l);
	nbuf[l] = 0;

	if (ft == ft_dir) {
	    backup_id = -1;
	} else if (is_backup) {
	    char * p = strrchr(nbuf, '.');
	    if (p == 0) backup_id = 1;
	    else {
		char *e = "";
		backup_id = strtoi(p+1, &e, 10);
		if (backup_id<=0 || *e) continue;
		*p = 0;
	    }
	}

	unfix_fname(nbuf2, sizeof(nbuf2), nbuf);
	if (*nbuf2 == 0) {
	    if (entry->d_name[0] != '.')
		printlog("Ignoring unconvertable map file name: %s", entry->d_name);
	    continue;
	}
	l = strlen(nbuf2);
	if (l>MAXLEVELNAMELEN) continue;

	if (matchstr_len != 0) {
	    if (is_backup != 2) {
		if (my_strcasestr(nbuf2, matchstr) == 0)
		    continue;
	    } else {
		if (strcasecmp(nbuf2, matchstr) != 0)
		    continue;
	    }
	}

	if (maps->count >= maps->size) {
	    if (maps->size==0) maps->size = 32;
	    maps->entry = realloc(maps->entry, (maps->size *= 2)*sizeof*(maps->entry));
	}
	maps->entry[maps->count].name = strdup(nbuf2);
	maps->entry[maps->count].backup_id = backup_id;
	maps->entry[maps->count].backup_id2 = backup_id;
	maps->count++;
    }
}
