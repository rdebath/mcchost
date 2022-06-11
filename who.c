
#include "who.h"

/*HELP who H_CMD
&T/who
List out the connected users
*/

#if INTERFACE
#define CMD_WHO \
    {N"who", &cmd_who}
#endif

void
cmd_who(UNUSED char * cmd, UNUSED char * arg)
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
	else
	    printf_chat("\\%s is on %s (%d,%d,%d)",
		c.name.c, shdat.client->levels[level_id].level.c,
		c.posn.x/32, c.posn.y/32, c.posn.z/32);
    }

    printf_chat("You are on %s (%d,%d,%d)",
	current_level_name, player_posn.x/32, player_posn.y/32, player_posn.z/32);
    if (users == 0)
	printf_chat("There are currently no other users");
}

