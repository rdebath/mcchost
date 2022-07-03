
#include "cmdannounce.h"

/*HELP announce,an H_CMD
&T/announce message
Display a really important message
Alias &T/an
*/

#if INTERFACE
#define CMD_ANNOUNCE \
    {N"announce", &cmd_announce}, {N"an", &cmd_announce, .dup=1}
#endif

void
cmd_announce(UNUSED char * cmd, char * arg)
{
    // TODO Permissions ? Attribution ?
    printf_chat("@~(100)%s", arg);
}

