
#include "cmdkick.h"

#if INTERFACE
#define CMD_KICK \
    {N"kick", &cmd_kick}
#endif

/*HELP kick H_CMD
&T/Kick [player] <reason>
Kick the player
The player name may be any unique substring
*/

void
cmd_kick(char * cmd, char *arg)
{
    char * str1 = strtok(arg, " ");
    char * str2 = strtok(0, "");

    if (!str1) return cmd_help(0, cmd);

    int ucount = 0;
    for(int mode = 0; mode < 3; mode++) {
	for(int i = 0; i < MAX_USER; i++)
	{
	    if (i == my_user_no) continue; // Me
	    client_entry_t c = shdat.client->user[i];

	    if (!c.active) continue;

	    if (mode == 0) {
		// Exact match
		if (strcmp(str1, c.name.c) != 0) continue; // Not them.
	    } else if (mode == 1) {
		// How many partial matches
		if (my_strcasestr(c.name.c, str1) != 0)
		    ucount++;
		continue;
	    } else // Partial match
		if (my_strcasestr(c.name.c, str1) == 0)
		    continue;

	    // c is player
	    cmd_payload_t msg = {0};
	    saprintf(msg.arg.c, "by %s%s%s", user_id, str2?": &f":"", str2?str2:"");
	    msg.cmd_token = cmd_token_kick;
	    send_ipc_cmd(1, i, &msg);
	    return;
	}
	if (mode == 1 && ucount == 0) break;
	if (mode == 1 && ucount > 1) {
	    printf_chat("&WCan't kick \"%s\" it matches %d users", str1, ucount);
	    return;
	}
    }
    printf_chat("&WCannot find user %s", str1);
    return;
}
