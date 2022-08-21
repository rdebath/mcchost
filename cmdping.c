
#include "cmdping.h"

/*HELP ping H_CMD
&T/ping
Display ping status
*/

#if INTERFACE
#define CMD_PING \
    {N"ping", &cmd_ping}
#endif

void
cmd_ping(char * UNUSED(cmd), char * UNUSED(arg))
{
    // TODO Permissions ? Attribution ?
    if (last_ping_ms == -1)
	printf_chat("&SNo ping measurements yet.");
    else
	printf_chat("&SLowest ping %dms, average %dms, highest %dms",
	   min_ping_ms, avg_ping_ms, max_ping_ms);
}

