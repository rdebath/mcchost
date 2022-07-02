#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cmdsetspawn.h"

#if INTERFACE
#define CMD_SETSPAWN {N"setspawn", &cmd_setspawn}
#endif

/*HELP setspawn H_CMD
&T/setspawn
Sets spawn location set to your current location and orientation
*/

void
cmd_setspawn(UNUSED char * cmd, UNUSED char * arg)
{
    level_prop->spawn.x = player_posn.x;
    level_prop->spawn.y = player_posn.y;	// TODO? Make this exact?
    level_prop->spawn.z = player_posn.z;
    level_prop->spawn.h = player_posn.h;
    level_prop->spawn.v = player_posn.v;

    level_prop->dirty_save = 1;
    level_prop->metadata_generation++;
    level_prop->last_modified = time(0);

    printf_chat("&SSpawn location set to your current location and orientation");
}
