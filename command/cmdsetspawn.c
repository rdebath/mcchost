
#include "cmdsetspawn.h"

#if INTERFACE
#define UCMD_SETSPAWN {N"setspawn", &cmd_setspawn, .map=1, CMD_PERM_LEVEL}
#endif

/*HELP setspawn H_CMD
&T/setspawn
Sets spawn location set to your current location and orientation
*/

void
cmd_setspawn(char * UNUSED(cmd),char * arg)
{
    char * str1 = strarg(arg);
    char * str2 = strarg(0);
    char * str3 = strarg(0);
    char * str4 = strarg(0);
    char * str5 = strarg(0);

    if (str1 && *str1) {
	int x = conv_ord(str1, player_posn.x, 16);
	int y = conv_ord(str2, player_posn.y, 0);
	int z = conv_ord(str3, player_posn.z, 16);
	int h = str4?atoi(str4)*256/360:0;
	int v = str5?atoi(str5)*256/360:0;

	level_prop->spawn = (xyzhv_t){x,y,z,h,v,0};

	printf_chat("&SSpawn location set");
    } else {
	level_prop->spawn.x = player_posn.x;
	level_prop->spawn.y = player_posn.y;
	level_prop->spawn.z = player_posn.z;
	level_prop->spawn.h = player_posn.h;
	level_prop->spawn.v = player_posn.v;

	printf_chat("&SSpawn location set to your current location and orientation");
    }

    level_prop->dirty_save = 1;
    level_prop->metadata_generation++;
    level_prop->last_modified = time(0);

    if (extn_setspawnpoint)
	send_setspawn_pkt(level_prop->spawn);
}
