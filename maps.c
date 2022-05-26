#include <string.h>
#include <dirent.h>
#include <time.h>

#include "maps.h"

// TODO qsort, paginate, filter. Based on arg1 && arg2 then arg1 || arg2?
// Use same list generator for goto (one result only).

/*HELP maps,levels,worlds
&T/maps
List out the available levels
*/

#if INTERFACE
#define CMD_MAPS \
    {N"maps", &cmd_maps}, {N"levels", &cmd_maps, .dup=1}, \
    {N"worlds", &cmd_maps, .dup=1}

#define MAXLEVELNAMELEN 32
#endif

void
cmd_maps(UNUSED char * cmd, char * arg)
{
    DIR *directory = opendir("map");
    if (!directory) {
	printf_chat("#No maps found... WTF where is %s‼", server.main_level);
	return;
    }

    printf_chat("&SLevels:");

    struct dirent *entry;
    char line_buf[NB_SLEN] = {0};
    int c = 0;

    while( (entry=readdir(directory)) )
    {

#ifdef _DIRENT_HAVE_D_TYPE
	if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN)
	    continue;
#endif

	int l = strlen(entry->d_name);
	if (l>3 && strcmp(entry->d_name+l-3, ".cw") == 0) {
	    l -= 3;
	    char nbuf[MAXLEVELNAMELEN*4];
	    char nbuf2[MAXLEVELNAMELEN+1];
	    if (l>sizeof(nbuf)-2) continue;
	    memcpy(nbuf, entry->d_name, l);
	    nbuf[l] = 0;
	    unfix_fname(nbuf2, sizeof(nbuf2), nbuf);
	    if (*nbuf2 == 0) continue;
	    l = strlen(nbuf2);

	    if (c != 0 && c + 2 + l > NB_SLEN-1-3) {
		printf_chat("&7%s,", line_buf);
		c = 0; *line_buf = 0;
	    }
	    if (c) {
		strcpy(line_buf+c, ", ");
		c += 2;
	    }
	    strcpy(line_buf+c, nbuf2);
	    c += l;
	}
    }

    if (c)
	printf_chat("&7%s", line_buf);

    closedir(directory);
}

