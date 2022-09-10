#include <stdio.h>
#include <string.h>

#include "cmdwho.h"

/*HELP who H_CMD
&T/who
List out the connected users
*/

#if INTERFACE
#define CMD_WHO \
    {N"who", &cmd_who}, \
    {N"pclients", &cmd_who, .nodup=1}, \
    {N"clients", &cmd_who, .dup=1}
#endif

void
cmd_who(char * cmd, char * UNUSED(arg))
{
    int users = 0;
    int show_software = 0;
    int show_locations = 1;

    // int my_level_id = shdat.client->user[my_user_no].on_level;
    if (strcmp(cmd, "pclients") == 0)
	show_software = 1, show_locations = 0;

    for(int i=0; i<MAX_USER; i++)
    {
	if (i == my_user_no && !show_software) continue; // Me
	client_entry_t c = shdat.client->user[i];
	if (!c.active) continue;
	users++;

	if (show_locations) {
	    char nbuf[256];
	    snprintf(nbuf, sizeof(nbuf), "%s%s", c.name.c, c.is_afk?" (AFK)":"");

	    int level_id = c.on_level;
	    if (level_id<0)
		printf_chat("\\%s is between levels", nbuf);
	    else if (shdat.client->levels[level_id].backup_id>0)
		printf_chat("\\%s is on museum %d of %s at (%d,%d,%d)",
		    nbuf,
		    shdat.client->levels[level_id].backup_id,
		    shdat.client->levels[level_id].level.c,
		    c.posn.x/32, c.posn.y/32, c.posn.z/32);
	    else if (shdat.client->levels[level_id].backup_id==0)
		printf_chat("\\%s is on %s at (%d,%d,%d)",
		    nbuf, shdat.client->levels[level_id].level.c,
		    c.posn.x/32, c.posn.y/32, c.posn.z/32);
	    else
		printf_chat("\\%s is in the void", nbuf);
	}

	if (show_software) {
	    if (c.client_cpe && *c.client_software.c)
		printf_chat("\\%s: %s", c.name.c, c.client_software.c);
	    else if (c.client_cpe)
		printf_chat("\\%s is using an unnamed CPE client", c.name.c);
	    else if (c.client_proto_ver == 7)
		printf_chat("\\%s is using Classic 0.28-0.30", c.name.c);
	    else if (c.client_proto_ver == 6)
		printf_chat("\\%s is using Classic 0.0.20-0.0.23", c.name.c);
	    else if (c.client_proto_ver == 5)
		printf_chat("\\%s is using Classic 0.0.19", c.name.c);
	    else if (c.client_proto_ver == 4)
		printf_chat("\\%s is using Classic 0.0.17-0.0.18", c.name.c);
	    else if (c.client_proto_ver == 3)
		printf_chat("\\%s is using Classic 0.0.16", c.name.c);
	    else
		printf_chat("\\%s is a bot", c.name.c);
	}
    }

    if (show_locations) {
	if (current_level_backup_id == 0)
	    printf_chat("You are on %s at (%d,%d,%d)",
		current_level_name, player_posn.x/32, player_posn.y/32, player_posn.z/32);
	else if (current_level_backup_id>0)
	    printf_chat("You are on museum %d of %s at (%d,%d,%d)",
		current_level_backup_id, current_level_name,
		player_posn.x/32, player_posn.y/32, player_posn.z/32);
	else
	    printf_chat("You are nowhere.");

	if (users == 0)
	    printf_chat("There are currently no other users");
    }
}
