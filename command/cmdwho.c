
#include "cmdwho.h"

/*HELP who,players H_CMD
&T/who
Lists name and level of all connected players
See &T/where&S for exact location of a player
*/

/*HELP pclients,clients H_CMD
&T/PClients
Lists the clients players are using, and who uses which client.
Shortcuts: /Clients
*/

#if INTERFACE
#define UCMD_WHO \
    {N"who", &cmd_who}, {N"players", &cmd_who, CMD_ALIAS}, \
    {N"pclients", &cmd_clients}, {N"clients", &cmd_clients, CMD_ALIAS}
#endif

void
cmd_who(char * UNUSED(cmd), char * UNUSED(arg))
{
    char linebuf[BUFSIZ];
    uint8_t level_dup[MAX_USER] = {0};

    printf_chat("Levels with players:");

    int users = 0;

    for(int i=0; i<MAX_USER; i++) {
        if (shdat.client->user[i].state.active == 1) {
            users ++;
            if (level_dup[i] == 0) {
                for(int j = i+1; j<MAX_USER; j++) {
                    if (shdat.client->user[j].state.active == 1 &&
                        shdat.client->user[i].state.on_level == shdat.client->user[j].state.on_level)
                    {
                        level_dup[j] = 1;
                    }
                }
            }
        }
    }

    for(int j = 0; j<MAX_USER; j++)
    {
	client_entry_t c1 = shdat.client->user[j];
	if (!c1.state.active) continue;
	if (level_dup[j]) continue;

	int level_id = c1.state.on_level;

	if (level_id<0 || level_id >= MAX_LEVEL)
	    saprintf(linebuf, "In the void");
	else if (shdat.client->levels[level_id].backup_id>0)
	    saprintf(linebuf, "Museum %d of %s",
		shdat.client->levels[level_id].backup_id,
		shdat.client->levels[level_id].level.c);
	else if (shdat.client->levels[level_id].backup_id==0)
	    saprintf(linebuf, "Level %s", shdat.client->levels[level_id].level.c);
	else
	    saprintf(linebuf, "In the void");

	int l = strlen(linebuf);
	l += snprintf(linebuf+l, sizeof(linebuf)-l, ": &f%s", c1.name.c);
	for(int i=j; i<MAX_USER; i++)
	{
	    client_entry_t c = shdat.client->user[i];
	    if (!c.state.active || !level_dup[i]) continue;
	    if (c1.state.on_level != c.state.on_level)
		continue;

	    if (l + strlen(c.name.c) + 4 > sizeof(linebuf)) {
		printf_chat("  %s", linebuf); l=0; *linebuf = 0;
	    }
	    l += snprintf(linebuf+l, sizeof(linebuf)-l, ", %s", c.name.c);
	}

	printf_chat("  %s", linebuf);
    }

    if (users <= 1)
	printf_chat("There are currently no other users");
    else
	printf_chat("There are a total of %d users", users);
}

void
cmd_clients(char * UNUSED(cmd), char * UNUSED(arg))
{
    char linebuf[BUFSIZ];
    uint8_t client_dup[MAX_USER] = {0};

    printf_chat("Players using:");

    int users = 0;

    for(int i=0; i<MAX_USER; i++) {
        if (shdat.client->user[i].state.active == 1) {
            users ++;
            if (client_dup[i] == 0) {
                for(int j = i+1; j<MAX_USER; j++) {
                    if (shdat.client->user[j].state.active == 1 &&
                        strcmp(shdat.client->user[i].client_software.c,
                        shdat.client->user[j].client_software.c) == 0)
                    {
                        client_dup[j] = 1;
                    }
                }
            }
        }
    }

    for(int j = 0; j<MAX_USER; j++)
    {
	client_entry_t c1 = shdat.client->user[j];
	if (!c1.state.active) continue;
	if (client_dup[j]) continue;

	int l = snprintf(linebuf, sizeof(linebuf), "%s: &f%s", c1.client_software.c, c1.name.c);
	for(int i=j; i<MAX_USER; i++)
	{
	    client_entry_t c = shdat.client->user[i];
	    if (!c.state.active || !client_dup[i]) continue;
	    if (strcmp(c1.client_software.c, c.client_software.c) != 0)
		continue;

	    if (l + strlen(c.name.c) + 4 > sizeof(linebuf)) {
		printf_chat("  %s", linebuf); l=0; *linebuf = 0;
	    }
	    l += snprintf(linebuf+l, sizeof(linebuf)-l, ", %s", c.name.c);
	}

	printf_chat("  %s", linebuf);
    }
}
