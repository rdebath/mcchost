

#include "cmdiload.h"

/*HELP iload,iniload H_CMD
&T/iload [name]
Load an ini file into the current level.
See &T/help inifile&S for syntax
Aliases: &T/iniload
*/
/*HELP isave,inisave H_CMD
&T/isave [name]
Save the properties of the current level into ini/${name}.ini
See &T/help inifile&S for syntax
Aliases: &T/inisave
*/
/*HELP ishow,inishow H_CMD
&T/ishow [name]
Show contents of ini/${name}.ini
Aliases: &T/inishow
*/

#if INTERFACE
#define CMD_INILOAD \
    {N"iniload", &cmd_iload, CMD_PERM_LEVEL}, {N"iload", &cmd_iload, .dup=1}, \
    {N"inisave", &cmd_isave}, {N"isave", &cmd_isave, .dup=1}, \
    {N"inishow", &cmd_ishow}, {N"ishow", &cmd_ishow, .dup=1}
#endif

void
cmd_iload(char * cmd, char * arg)
{
    if (!arg || !*arg)
	return printf_chat("&WUsage: /%s filename", cmd);

    int ro = level_prop->readonly;
    char fixedname[200], buf[256];
    fix_fname(fixedname, sizeof(fixedname), arg);
    saprintf(buf, "ini/%s.ini", fixedname);

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

void
cmd_isave(char * cmd, char * arg)
{
    if (!arg || !*arg)
	return printf_chat("&WUsage: /%s filename", cmd);

    char fixedname[200], buf2[256];
    fix_fname(fixedname, sizeof(fixedname), arg);
    saprintf(buf2, "ini/%s.ini", fixedname);

    if (access(buf2, F_OK) == 0) {
	printf_chat("#&WConfig file %s not overwritten", buf2);
	return;
    }

    save_ini_file(level_ini_fields, buf2, 0);

    printf_chat("#&SConfig saved to %s", buf2);
}

void
cmd_ishow(char * cmd, char * arg)
{
    if (!arg || !*arg)
	return printf_chat("&WUsage: /%s filename", cmd);

    char fixedname[200], buf2[256];
    fix_fname(fixedname, sizeof(fixedname), arg);
    saprintf(buf2, "ini/%s.ini", fixedname);

    if (access(buf2, F_OK) != 0) {
        printf_chat("#&WConfig file %s does not exist", buf2);
        return;
    }

    char buf[4096];

    FILE * fd = fopen(buf2, "r");
    if (fd) {
        while(fgets(buf, sizeof(buf), fd)) {
            int l = strlen(buf);
            char * p = buf+l;
            if (l != 0 && p[-1] == '\n') { p[-1] = 0; l--; }
            convert_to_cp437(buf, &l);
            post_chat(-1, 0, 0, buf, l);
        }
        fclose(fd);
        return;
    }
}
