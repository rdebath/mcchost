
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "ini_load.h"

/*HELP cload,confload H_CMD
&T/cload [name]
Load an ini file into the current level.
Aliases: &T/confload
*/
/*HELP csave,confsave H_CMD
&T/csave [name]
Save the properties of the current level into into ini/${name}.ini
Aliases: &T/confsave
*/

#if INTERFACE
#define CMD_INILOAD \
    {N"confload", &cmd_cload}, {N"cload", &cmd_cload, .dup=1}, \
    {N"confsave", &cmd_csave}, {N"csave", &cmd_csave, .dup=1}
#endif

void
cmd_cload(char * cmd, char * arg)
{
    if (!client_trusted)
	printf_chat("&WUsage: /%s [Auth] filename", cmd);
    else if (!arg || !*arg)
	printf_chat("&WUsage: /%s filename", cmd);
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
cmd_csave(char * cmd, char * arg)
{
    if (!arg || !*arg)
	printf_chat("&WUsage: /%s filename", cmd);
    else {
	char fixedname[200], buf2[256];
	fix_fname(fixedname, sizeof(fixedname), arg);
	snprintf(buf2, sizeof(buf2), "ini/%s.ini", fixedname);

	if (access(buf2, F_OK) == 0) {
	    printf_chat("&WConfig file %s not overwritten", buf2);
	    return;
	}

	save_ini_file(level_ini_fields, buf2);

	printf_chat("&SConfig saved to %s", buf2);
    }
    return;
}
