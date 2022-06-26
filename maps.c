#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

#include "maps.h"

/*HELP maps,levels,worlds H_CMD
&T/maps
List out the available levels
use &T/maps [number]&S to start at that position
and &T/maps all&S to show all maps.
*/

#if INTERFACE
#define CMD_MAPS \
    {N"maps", &cmd_maps}, {N"levels", &cmd_maps, .dup=1}, \
    {N"worlds", &cmd_maps, .dup=1}

#define MAXLEVELNAMELEN 32
#endif

static int
pstrcasecmp(const void *p1, const void *p2)
{
    char **e1 = (char * *)p1;
    char **e2 = (char * *)p2;

    int r = strnatcasecmp(*e1, *e2);
    if (r) return r;
    /* We can get duplicates, this stabilises the final order. */
    return strcmp(*e1, *e2);
}

void
cmd_maps(UNUSED char * cmd, char * arg)
{
    DIR *directory = opendir("map");
    if (!directory) {
	printf_chat("#No maps found... WTF where is %s‼", main_level());
	return;
    }

    int start = 0;
    if (!arg || *arg == 0) start = 1;
    else if (strcasecmp(arg, "all") == 0) start = 0;
    else {
	start = atoi(arg);
	if (start <= 0) {
	    printf_chat("&eInput must be either \"all\" or an integer.");
	    return;
	}
    }

    struct dirent *entry;
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
	maplist[maplist_cnt++] = strdup(nbuf2);
    }
    closedir(directory);

    qsort(maplist, maplist_cnt, sizeof*maplist, pstrcasecmp);

    char line_buf[NB_SLEN] = {0};
    int c = 0, end = maplist_cnt;
    if (start == 0) end = maplist_cnt;
    else { start--; end = start + 30; }
    if (end>maplist_cnt) end = maplist_cnt;

    if (!maplist_cnt) {
	printf_chat("&WNo levels have been saved yet");
	return;
    }

    printf_chat("&SShowing levels %d-%d (out of %d)", start+1,end,maplist_cnt);
    for(int i = start; i<end; i++) {
	char * s = maplist[i];
	int l = strlen(s);

	if (c != 0 && c + 2 + l > NB_SLEN-1-3) {
	    printf_chat("&7%s,", line_buf);
	    c = 0; *line_buf = 0;
	}
	if (c) {
	    strcpy(line_buf+c, ", ");
	    c += 2;
	}
	strcpy(line_buf+c, s);
	c += l;
    }
    if (c)
	printf_chat("&7%s", line_buf);

    for(int i = 0; i<maplist_cnt; i++)
	free(maplist[i]);
    free(maplist);
}

