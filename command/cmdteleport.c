
#include "cmdteleport.h"

#if INTERFACE
#define UCMD_TP \
    {N"tp", &cmd_tp}, \
    {N"move", &cmd_tp, CMD_ALIAS}, \
    {N"teleport", &cmd_tp, CMD_ALIAS}, \
    {N"back", &cmd_back}, \
    {N"ascend", &cmd_ascend}, \
    {N"descend", &cmd_descend}, \
    {N"spawn", &cmd_spawn}
#endif

/*HELP tp,move,teleport H_CMD
&T/TP [x y z] <yaw> <pitch>
Teleports yourself to the given block coordinates.
Use ~ before a coordinate to move relative to current position
Use a decimal value for sub block positioning
&T/TP [player]
Teleport yourself to the level and position of the player
The player name may be any unique substring
Shortcuts: &T/Move, /Teleport
*/

/*HELP spawn H_CMD
&T/Spawn
Teleports you to the spawn location of the level.
*/

/*HELP back H_CMD
&T/Back
Takes you back to the position you were before the last teleportation.
*/

/*HELP ascend H_CMD
&T/Ascend
Teleports you to the first free space above you.
*/

/*HELP descend H_CMD
&T/Descend
Teleports you to the first free space below you.
*/

xyzhv_t player_back_posn = {0};

void
cmd_spawn(char * UNUSED(cmd), char *UNUSED(arg))
{
    if (!level_prop || (level_prop->hacks_flags & 8)) {
	printf_chat("&cRespawning is currently disabled");
	return;
    }

    send_posn_pkt(-1, &player_posn, level_prop->spawn);
    player_posn = level_prop->spawn;
}

void
cmd_tp(char * cmd, char *arg)
{
    if (!level_prop || (level_prop->hacks_flags & 8)) {
	printf_chat("&cTeleport is currently disabled");
	return;
    }

    char * str1 = strarg(arg);
    char * str2 = strarg(0);
    char * str3 = strarg(0);
    char * str4 = strarg(0);
    char * str5 = strarg(0);

    if (str1 != 0 && str2 == 0) {
	int uid = find_online_player(str1, 0, 0);
	if (uid < 0) return;

	int my_level = shdat.client->user[my_user_no].state.on_level;
	client_entry_t c = shdat.client->user[uid];

	if (!c.state.active) return;
	if (c.state.on_level == my_level) {
	    send_posn_pkt(-1, &player_posn, c.state.posn);
	    player_posn = c.state.posn;
	    return;
	}

	client_level_t lvl = {0};
	if (c.state.on_level >= 0 && c.state.on_level < MAX_LEVEL)
	    lvl = shdat.client->levels[c.state.on_level];

	if (!lvl.loaded || lvl.backup_id < 0) {
	    cmd_void("",0);
	    return;
	}

	xyzhv_t *ppos = 0;
	if (c.state.posn.valid) ppos = &c.state.posn;

	if (!direct_teleport(lvl.level.c, lvl.backup_id, ppos)) {
	    if (lvl.backup_id == 0)
		printf_chat("&WCannot teleport to user %s on %s", c.name.c, lvl.level.c);
	    else
		printf_chat("&WCannot teleport to user %s on museum of %s", c.name.c, lvl.level.c);
	}
	return;

    } else if (str3 != 0) {
	int x = conv_ord(str1, player_posn.x, 16);
	int y = conv_ord(str2, player_posn.y, 0);
	int z = conv_ord(str3, player_posn.z, 16);
	int h = str4?atoi(str4)*256/360:player_posn.h;
	int v = str5?atoi(str5)*256/360:player_posn.v;

	xyzhv_t newposn = {x,y,z,h,v,1};
	player_back_posn = player_posn;

	send_posn_pkt(-1, &player_posn, newposn);
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
cmd_back(char * UNUSED(cmd), char *UNUSED(arg))
{
    if (!level_prop || (level_prop->hacks_flags & 8)) {
	printf_chat("&cTeleport is currently disabled");
	return;
    }

    if (!player_back_posn.valid) {
	printf_chat("You have not teleported anywhere yet");
	return;
    }

    send_posn_pkt(-1, &player_posn, player_back_posn);
    player_posn = player_back_posn;
}

void
cmd_ascend(char * UNUSED(cmd), char *UNUSED(arg))
{
    if (!level_prop || !level_blocks || (level_prop->hacks_flags & 8)) {
	printf_chat("&c/Ascend is currently disabled");
	return;
    }

    xyzhv_t newposn = player_posn;
    int x,y,z;

    x = player_posn.x/32; // check range [0..cells_x)
    y = (player_posn.y+16)/32;// Add half a block for slabs
    z = player_posn.z/32;
    if (x<0 || z<0 || x >= level_prop->cells_x || z >= level_prop->cells_z) {
	printf_chat("There are no blocks above to ascend to.");
	return;
    } else {
	if (y<0) y = -1;
	for(;;) {
	    y++;
	    if (y >= level_prop->cells_y) {
		printf_chat("There are no blocks above to ascend to.");
		return;
	    }
	    if (level_blocks[World_Pack(x, y, z)] != Block_Air)
		break;
	}
	for(; y<level_prop->cells_y; y++) {
	    uintptr_t index = World_Pack(x, y, z);
	    if (level_blocks[index] == Block_Air &&
		(y+1 == level_prop->cells_y ||
		    level_blocks[World_Pack(x, y+1, z)] == Block_Air))
		break;
	}
    }

    newposn.y = y*32 + 16;
    send_posn_pkt(-1, &player_posn, newposn);
    player_posn = newposn;
}

void
cmd_descend(char * UNUSED(cmd), char *UNUSED(arg))
{
    if (!level_prop || (level_prop->hacks_flags & 8)) {
	printf_chat("&c/Descend is currently disabled");
	return;
    }

    xyzhv_t newposn = player_posn;
    int x,y,z;

    x = player_posn.x/32; // check range [0..cells_x)
    y = (player_posn.y+16)/32;// Add half a block for slabs
    z = player_posn.z/32;
    if (x<0 || z<0 || x >= level_prop->cells_x || z >= level_prop->cells_z) {
	printf_chat("No free spaces found below you");
	return;
    } else {
	if (y> level_prop->cells_y) y = level_prop->cells_y;
	for(;;) {
	    y--;
	    if (y < 1) {
		printf_chat("No free spaces found below you");
		return;
	    }
	    if (level_blocks[World_Pack(x, y, z)] != Block_Air)
		break;
	}
	for(; y>=0; y--) {
	    if (y>=1) {
		uintptr_t index = World_Pack(x, y, z);
		if (level_blocks[index] == Block_Air &&
		    level_blocks[World_Pack(x, y-1, z)] == Block_Air)
		    break;
	    }
	}
	if (y<0) {
	    printf_chat("No free spaces found below you");
	    return;
	}
    }

    newposn.y = y*32 + 16;
    send_posn_pkt(-1, &player_posn, newposn);
    player_posn = newposn;
}
