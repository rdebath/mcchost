
#include "cmdwho.h"

/*HELP who H_CMD
&T/who
List out the connected users
*/

#if INTERFACE
#define CMD_WHO \
    {N"who", &cmd_who}, {N"info", &cmd_info, .dup=1, .nodup=1}
#endif

void
cmd_who(char * UNUSED(cmd), char * UNUSED(arg))
{
    int users = 0;
    // int my_level_id = shdat.client->user[my_user_no].on_level;

    for(int i=0; i<MAX_USER; i++)
    {
	if (i == my_user_no) continue; // Me
	client_entry_t c = shdat.client->user[i];
	if (!c.active) continue;
	users++;

	int level_id = c.on_level;
	if (level_id<0)
	    printf_chat("\\%s is between levels", c.name.c);
	else if (shdat.client->levels[level_id].museum_id>0)
	    printf_chat("\\%s is on museum %d of %s at (%d,%d,%d)",
		shdat.client->levels[level_id].museum_id,
		c.name.c, shdat.client->levels[level_id].level.c,
		c.posn.x/32, c.posn.y/32, c.posn.z/32);
	else if (shdat.client->levels[level_id].museum_id==0)
	    printf_chat("\\%s is on %s at (%d,%d,%d)",
		c.name.c, shdat.client->levels[level_id].level.c,
		c.posn.x/32, c.posn.y/32, c.posn.z/32);
	else
	    printf_chat("\\%s is in the void", c.name.c);
    }

    if (current_level_museum_id == 0)
	printf_chat("You are on %s at (%d,%d,%d)",
	    current_level_name, player_posn.x/32, player_posn.y/32, player_posn.z/32);
    else if (current_level_museum_id>0)
	printf_chat("You are on museum %d of %s at (%d,%d,%d)",
	    current_level_museum_id, current_level_name,
	    player_posn.x/32, player_posn.y/32, player_posn.z/32);
    else
	printf_chat("You are nowhere.");

    if (users == 0)
	printf_chat("There are currently no other users");
}

void
cmd_info(char * UNUSED(cmd), char * UNUSED(arg))
{
}
