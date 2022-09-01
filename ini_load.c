
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ini_load.h"

/*HELP load H_CMD
&T/load [name]
Load an ini file into the current level.
*/
/*HELP save H_CMD
&T/save [name]
Save the properties of the current level into into ini/${name}.ini
*/

#if INTERFACE
#define CMD_INILOAD \
    {N"load", &cmd_load}, {N"save", &cmd_save}
#endif

void
cmd_load(char * UNUSED(cmd), char * arg)
{
    if (!client_trusted)
	printf_chat("&WUsage: /load [Auth] filename");
    else if (!arg || !*arg)
	printf_chat("&WUsage: /load filename");
    else {
	int ro = level_prop->readonly;
	char fixedname[200], buf[256];
	fix_fname(fixedname, sizeof(fixedname), arg);
	snprintf(buf, sizeof(buf), "ini/%s.ini", fixedname);

	lock_fn(level_lock);
	int rv = load_ini_file(level_ini_fields, buf, 0, 1);
	unlock_fn(level_lock);

	if (rv == 0) {
	    level_prop->readonly = ro;	// Use /set command

	    printf_chat("&SFile loaded%s%s",
		level_prop->readonly?" to readonly level":"",
		level_prop->force_save&&level_prop->readonly?", but will save anyway":"");

	    level_prop->dirty_save = 1;
	    level_prop->metadata_generation++;
	    level_prop->last_modified = time(0);
	}
    }
}

void
cmd_save(char * UNUSED(cmd), char * arg)
{
    if (!client_trusted)
	printf_chat("&WUsage: /save [Auth] filename");
    else if (!arg || !*arg)
	printf_chat("&WUsage: /save filename");
    else {
	char fixedname[200], buf2[256];
	fix_fname(fixedname, sizeof(fixedname), arg);
	snprintf(buf2, sizeof(buf2), "ini/%s.ini", fixedname);

	save_ini_file(level_ini_fields, buf2);

	printf_chat("&SConfig saved to %s", buf2);
    }
    return;
}
