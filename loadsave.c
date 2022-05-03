#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "loadsave.h"

/*TODO: HELP load
&T/load [name]
*/
/*TODO: HELP save
&T/save [name]
*/

#if INTERFACE
#define CMD_LOADSAVE  {N"load", &cmd_load}, {N"save", &cmd_save}
#endif

void
cmd_load(UNUSED char * cmd, char * arg)
{
    if (!client_ipv4_localhost)
	printf_chat("&WUsage: /load [Auth] filename");
    else if (!arg || !*arg)
	printf_chat("&WUsage: /load filename");
    else {
	char buf1[256], buf2[256], *s, *d;
	for(s=arg, d=buf1; *s && d<buf1+sizeof(buf1)-1; s++) {
	    if ((*s>='A' && *s<='Z') || (*s>='a' && *s<='z') ||
		(*s>='0' && *s<='9') || *s=='_' || *s=='.')
		*d++ = *s;
	}
	*d = 0;

	snprintf(buf2, sizeof(buf2), "backup/%.200s.ini", buf1);

	lock_shared();
	int x = level_prop->cells_x;
	int y = level_prop->cells_y;
	int z = level_prop->cells_z;
	load_ini_file(level_ini_fields, buf2, 0);

	// Restore map size to match blocks.
	level_prop->cells_x = x;
	level_prop->cells_y = y;
	level_prop->cells_z = z;
	unlock_shared();

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
	char buf1[256], buf2[256], *s, *d;
	for(s=arg, d=buf1; *s && d<buf1+sizeof(buf1)-1; s++) {
	    if ((*s>='A' && *s<='Z') || (*s>='a' && *s<='z') ||
		(*s>='0' && *s<='9') || *s=='_' || *s=='.')
		*d++ = *s;
	}
	*d = 0;

	snprintf(buf2, sizeof(buf2), "backup/%.200s.cw", buf1);
	save_map_to_file(buf2);
	snprintf(buf2, sizeof(buf2), "backup/%.200s.ini", buf1);
	save_ini_file(level_ini_fields, buf2);
	printf_chat("&SFile saved");
    }
    return;
}
