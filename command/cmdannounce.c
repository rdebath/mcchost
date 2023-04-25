
#include "cmdannounce.h"

/*HELP announce,an H_CMD
&T/announce message
Display a really important message
Alias &T/an
*/

#if INTERFACE
#define UCMD_ANNOUNCE \
    {N"announce", &cmd_announce, CMD_HELPARG}, {N"an", &cmd_announce, CMD_ALIAS}
#endif

void
cmd_announce(char * UNUSED(cmd), char * arg)
{
    // TODO Permissions ?
    printf_chat("@~(102)&8from %s", player_list_name.c);
    printf_chat("@~(100)%s", arg);
}
