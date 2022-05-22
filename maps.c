#include <string.h>
#include <dirent.h>

#include "maps.h"

/*HELP maps,levels,worlds
&T/maps
List out the available levels
*/

#if INTERFACE
#define CMD_MAPS \
    {N"maps", &cmd_maps}, {N"levels", &cmd_maps, .dup=1}, \
    {N"worlds", &cmd_maps, .dup=1}
#endif

void
cmd_maps(UNUSED char * cmd, char * arg)
{
    DIR *directory = opendir("map");
    if (!directory) {
	printf_chat("#No maps found... WTF where is %s‼", server.main_level);
	return;
    }

    struct dirent *entry;
    char line_buf[NB_SLEN] = {0};
    int c = 0;

    // Todo? qsort this. TODO: Paginate.
    while( (entry=readdir(directory)) )
    {
	int l = strlen(entry->d_name);
	if (l>3 && strcmp(entry->d_name+l-3, ".cw") == 0) {
	    l -= 3;
	    if (l > NB_SLEN-2) continue; // NOPE!
#ifdef _DIRENT_HAVE_D_TYPE
	    if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN)
		continue;
#endif

	    if (c + 3 + l >= NB_SLEN) {
		printf_chat("#\\%s,", line_buf);
		c = 0; *line_buf = 0;
	    }
	    if (c) {
		strcpy(line_buf+c, ", ");
		c += 2;
	    }
	    strncpy(line_buf+c, entry->d_name, l); // Trims .cw from end.
	    c += l;
	    line_buf[c] = 0;
	}
    }

    if (c)
	printf_chat("#\\%s", line_buf);

    closedir(directory);
}

