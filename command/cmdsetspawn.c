
#include "cmdsetspawn.h"

#if INTERFACE
#define UCMD_SETSPAWN {N"setspawn", &cmd_setspawn, .map=1, CMD_PERM_LEVEL}
#endif

/*HELP setspawn H_CMD
&T/setspawn
Sets spawn location set to your current location and orientation
*/

void
cmd_setspawn(char * UNUSED(cmd),char * UNUSED(arg))
{
    level_prop->spawn.x = player_posn.x;
    level_prop->spawn.y = player_posn.y;	// TODO? Make this exact?
    level_prop->spawn.z = player_posn.z;
    level_prop->spawn.h = player_posn.h;
    level_prop->spawn.v = player_posn.v;

    level_prop->dirty_save = 1;
    level_prop->metadata_generation++;
    level_prop->last_modified = time(0);

    if (extn_setspawnpoint)
	send_setspawn_pkt(level_prop->spawn);

    printf_chat("&SSpawn location set to your current location and orientation");
}
