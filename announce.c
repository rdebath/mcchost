#include <stdlib.h>
#include <string.h>

#include "announce.h"

/*HELP announce,an
&T/announce messgae
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
    printf_chat("@~(100)%s", arg);
}

