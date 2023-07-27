
#include "cmdwho.h"

/*HELP who H_CMD
&T/who
List out the connected users
*/

#if INTERFACE
#define UCMD_WHO \
    {N"who", &cmd_who}, {N"players", &cmd_who, CMD_ALIAS}, \
    {N"pclients", &cmd_clients}, {N"clients", &cmd_clients, CMD_ALIAS}
#endif

void
cmd_who(char * UNUSED(cmd), char * UNUSED(arg))
{
    int users = 0;

    for(int i=0; i<MAX_USER; i++)
    {
	client_entry_t c = shdat.client->user[i];
	if (i == my_user_no) continue;
	if (!c.state.active) continue;
	users++;

	char nbuf[256];
	saprintf(nbuf, "%s&S%s", c.listname.c, c.state.is_afk?" (AFK)":"");

	int level_id = c.state.on_level;
	if (level_id<0)
	    printf_chat("\\%s is between levels", nbuf);
	else if (shdat.client->levels[level_id].backup_id>0)
	    printf_chat("\\%s is on museum %d of %s at (%d,%d,%d)",
		nbuf,
		shdat.client->levels[level_id].backup_id,
		shdat.client->levels[level_id].level.c,
		c.state.posn.x/32, c.state.posn.y/32, c.state.posn.z/32);
	else if (shdat.client->levels[level_id].backup_id==0)
	    printf_chat("\\%s is on %s at (%d,%d,%d)",
		nbuf, shdat.client->levels[level_id].level.c,
		c.state.posn.x/32, c.state.posn.y/32, c.state.posn.z/32);
	else
	    printf_chat("\\%s is in the void", nbuf);
    }

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
    else
	printf_chat("There are a total of %d users", users+1);
}

void
cmd_clients(char * UNUSED(cmd), char * UNUSED(arg))
{
    char linebuf[BUFSIZ];
    printf_chat("Players using:");

    int users = 0;

    for(int i=0; i<MAX_USER; i++)
        shdat.client->user[i].client_dup = 0;

    for(int i=0; i<MAX_USER; i++) {
        if (shdat.client->user[i].state.active == 1) {
            users ++;
            if (shdat.client->user[i].client_dup == 0) {
                for(int j = i+1; j<MAX_USER; j++) {
                    if (shdat.client->user[j].state.active == 1 &&
                        strcmp(shdat.client->user[i].client_software.c,
                        shdat.client->user[j].client_software.c) == 0)
                    {
                        shdat.client->user[j].client_dup = 1;
                    }
                }
            }
        }
    }

    for(int j = 0; j<MAX_USER; j++)
    {
	client_entry_t c1 = shdat.client->user[j];
	if (!c1.state.active) continue;
	if (c1.client_dup) continue;

	int l = snprintf(linebuf, sizeof(linebuf), "%s: &f%s", c1.client_software.c, c1.name.c);
	for(int i=j; i<MAX_USER; i++)
	{
	    client_entry_t c = shdat.client->user[i];
	    if (!c.state.active || !c.client_dup) continue;
	    if (strcmp(c1.client_software.c, c.client_software.c) != 0)
		continue;

	    l += snprintf(linebuf+l, sizeof(linebuf)-l, ", %s", c.name.c);
	}

	printf_chat("  %s", linebuf);
    }

}
