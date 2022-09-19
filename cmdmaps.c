#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
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
*/

#if INTERFACE
#define CMD_MAPS \
    {N"maps", &cmd_maps}, {N"levels", &cmd_maps, .dup=1}, \
    {N"worlds", &cmd_maps, .dup=1}

#define MAXLEVELNAMELEN 32

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
    char * ar1 = strtok(arg, " ");
    char * ar2 = strtok(0, "");

    if (!arg || *arg == 0) start = 1;
    else {
	if (strcasecmp(ar1, "all") == 0) start = 0;
	else if (strcasecmp(ar1, "backup") == 0) {
	    backups = 1;
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
    if (!backups) {
	DIR *directory = opendir("map");
	if (directory) {
	    read_maps(&maps, 0, directory, ar2);
	    closedir(directory);
	}
    }
    if (backups) {
	DIR *directory = opendir(LEVEL_BACKUP_DIR_NAME);
	if (directory) {
	    read_maps(&maps, 1, directory, ar2);
	    closedir(directory);
	}
    }

    qsort(maps.entry, maps.count, sizeof(*maps.entry), pmaplistentrycmp);

    char line_buf[NB_SLEN] = {0};
    int c = 0, end = maps.count;
    if (start == 0) end = maps.count;
    else { start--; end = start + 30; }
    if (end>maps.count) end = maps.count;

    if (!maps.count) {
	printf_chat("&WNo levels have been saved yet");
	return;
    }

    printf_chat("&SShowing levels %d-%d (out of %d)", start+1,end,maps.count);
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

	    if (m1 == m2)
		l += sprintf(num, "[%d]", maps.entry[i].backup_id);
	    else
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

    while( (entry=readdir(directory)) )
    {

#if defined(_DIRENT_HAVE_D_TYPE) && defined(DT_REG) && defined(DT_UNKNOWN)
	if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN)
	    continue;
#endif
	int l = strlen(entry->d_name);
	if (l<=3 || strcmp(entry->d_name+l-3, ".cw") != 0) continue;

	char nbuf[MAXLEVELNAMELEN*4+sizeof(int)*3+3];
	char nbuf2[MAXLEVELNAMELEN+1];
	int backup_id = 0;

	l -= 3;
	if (l>sizeof(nbuf)-2) continue;
	memcpy(nbuf, entry->d_name, l);
	nbuf[l] = 0;

	if (is_backup) {
	    char * p = strrchr(nbuf, '.');
	    if (p == 0) continue;
	    backup_id = atoi(p+1);
	    if (backup_id<=0) continue;
	    *p = 0;
	}

	unfix_fname(nbuf2, sizeof(nbuf2), nbuf);
	if (*nbuf2 == 0) continue;
	l = strlen(nbuf2);
	if (l>MAXLEVELNAMELEN) continue;

	if (matchstr != 0 && *matchstr) {
	    if (my_strcasestr(nbuf2, matchstr) == 0)
		continue;
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
