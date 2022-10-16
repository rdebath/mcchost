
#include "cmdteleport.h"

#if INTERFACE
#define CMD_TP \
    {N"tp", &cmd_tp}, \
    {N"move", &cmd_tp, .dup=1}, \
    {N"teleport", &cmd_tp, .dup=1}, \
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

void
cmd_spawn(char * UNUSED(cmd), char *UNUSED(arg))
{
    if (!level_prop) return;

    send_posn_pkt(255, &player_posn, level_prop->spawn);
    player_posn = level_prop->spawn;
}

void
cmd_tp(char * cmd, char *arg)
{
    char * str1 = strtok(arg, " ");
    char * str2 = strtok(0, " ");
    char * str3 = strtok(0, " ");
    char * str4 = strtok(0, " ");
    char * str5 = strtok(0, " ");

    if (str1 != 0 && str2 == 0) {
	int uid = find_online_player(str1);
	if (uid < 0) return;

	int my_level = shdat.client->user[my_user_no].on_level;
	client_entry_t c = shdat.client->user[uid];

	if (!c.active) return;
	if (c.on_level == my_level) {
	    send_posn_pkt(255, &player_posn, c.posn);
	    player_posn = c.posn;
	    return;
	}

	client_level_t lvl = {0};
	if (c.on_level >= 0 && c.on_level < MAX_LEVEL)
	    lvl = shdat.client->levels[c.on_level];

	if (!lvl.loaded || lvl.backup_id < 0) {
	    printf_chat("&WUser %s is lost in the void", c.name.c);
	    return;
	}

	xyzhv_t *ppos = 0;
	if (c.posn.valid) ppos = &c.posn;

	if (!direct_teleport(lvl.level.c, lvl.backup_id, ppos)) {
	    if (lvl.backup_id == 0)
		printf_chat("&WCannot teleport to user %s on %s", c.name.c, lvl.level.c);
	    else
		printf_chat("&WCannot teleport to user %s on museum of %s", c.name.c, lvl.level.c);
	}
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

int
find_online_player(char * user_txt)
{
    int ucount = 0;
    for(int mode = 0; mode < 3; mode++) {
	for(int i = 0; i < MAX_USER; i++)
	{
	    if (i == my_user_no) continue; // Me
	    client_entry_t c = shdat.client->user[i];

	    if (!c.active) continue;

	    if (mode == 0) {
		// Exact match
		if (strcmp(user_txt, c.name.c) != 0) continue; // Not them.
	    } else if (mode == 1) {
		// How many partial matches
		if (my_strcasestr(c.name.c, user_txt) != 0)
		    ucount++;
		continue;
	    } else // Partial match
		if (my_strcasestr(c.name.c, user_txt) == 0)
		    continue;

	    return i;
	}
	if (mode == 1 && ucount == 0) break;
	if (mode == 1 && ucount > 1) {
	    printf_chat("&WCan't identify user \"%s\" that matches %d users", user_txt, ucount);
	    return -1;
	}
    }
    printf_chat("&WCannot find user %s", user_txt);
    return -1;
}
