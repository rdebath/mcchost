#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tp.h"

#if INTERFACE
#define CMD_TP \
    {N"tp", &cmd_tp}, \
    {N"spawn", &cmd_spawn}
#endif

/*HELP tp,move,teleport H_CMD
&T/TP [x y z] <yaw> <pitch>
Teleports yourself to the given block coordinates.
Use ~ before a coordinate to move relative to current position
Use a decimal value for sub block positioning.
&T/TP [player]
Teleports yourself to that player.
Shortcuts: &T/Move, /Teleport, /TPP for /TP -precise
*/

/*HELP spawn H_CMD
&T/Spawn
Teleports you to the spawn location of the level.
*/

void
cmd_tp(char * cmd, char *arg)
{
    char * str1 = strtok(arg, " ");
    char * str2 = strtok(0, " ");
    char * str3 = strtok(0, " ");
    char * str4 = strtok(0, " ");
    char * str5 = strtok(0, " ");

    if (str1 != 0 && str2 == 0) {
	int my_level = shdat.client->user[my_user_no].on_level;
	for(int i = 0; i < MAX_USER; i++)
	{
	    if (i == my_user_no) continue; // Me
	    client_entry_t c = shdat.client->user[i];

	    c.visible = (c.active && c.on_level == my_level);
	    if (!c.visible) continue; // Wrong level, etc.

	    // TODO --> partial match.
	    if (strcmp(str1, c.name.c) != 0) continue; // Not them.

	    send_posn_pkt(255, &player_posn, c.posn);
	    player_posn = c.posn;
	    return;
	}
	printf_chat("&WCannot find user %s", str1);
	return;
    } else if (str3 != 0) {
	int x = conv_ord(str1, player_posn.x, 16);
	int y = conv_ord(str2, player_posn.y, 51);
	int z = conv_ord(str3, player_posn.z, 16);
	int h = str4?atoi(str4)*256/360:player_posn.h;
	int v = str5?atoi(str5)*256/360:player_posn.v;

	xyzhv_t newposn = {x,y,z,h,v,1};

	send_posn_pkt(255, &player_posn, newposn);
	player_posn = newposn;

    } else
	return cmd_help(0, cmd);
}

int
conv_ord(char * s, int ref, int ioff)
{
    if (!s) return ref;
    if (*s == '~') { s++; ioff = 0; } else ref = 0;
    if (strchr(s, '.') != 0) ioff = 0;
    if (*s == '+') s++;

    double f = strtod(s, 0);
    return (int)(f*32) + ref + ioff;
}

void
cmd_spawn(char * UNUSED(cmd), char *UNUSED(arg))
{
    if (!level_prop) return;

    send_posn_pkt(255, &player_posn, level_prop->spawn);
    player_posn = level_prop->spawn;
}
