
#include "cmdoverseer.h"
/*HELP os,overseer,realm
&T/os [args]&S
The &Toverseer&S command permission hack is not required on this server. Please use the normal commands directly.
&T/newlvl +&S
&T/goto +&S
&T/restore 42&S
Etc ...
*/

#if INTERFACE
#define UCMD_OVERSEER \
    {N"os", &cmd_overseer, CMD_HELPARG}, \
    {N"overseer", &cmd_overseer, CMD_ALIAS}, \
    {N"realm", &cmd_overseer, CMD_ALIAS}
#endif

void
cmd_overseer(char * cmd, char * arg)
{
    char * subcmd = strarg(arg);
    if (strcasecmp(subcmd, "go") == 0)
	cmd_goto("goto", "+");
    else
	cmd_help("", cmd);
}
