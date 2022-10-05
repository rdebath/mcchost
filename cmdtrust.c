
#include "cmdtrust.h"

#if INTERFACE
#define CMD_TRUST \
    {N"trust", &cmd_trust}
#endif

/*HELP trust H_CMD
&T/TRUST [player]
Turns off anit-grief for player.
*/

void
cmd_trust(char * cmd, char *arg)
{
    char * str1 = strtok(arg, " ");

    if (!str1)
	return cmd_help(0, cmd);

    int my_level = shdat.client->user[my_user_no].on_level;

    int ucount = 0;
    for(int mode = 0; mode < 3; mode++) {
	for(int i = 0; i < MAX_USER; i++)
	{
	    client_entry_t c = shdat.client->user[i];

	    c.visible = (c.active && c.on_level == my_level);
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

	    int v = !c.trusted;
	    shdat.client->user[i].trusted = v;
	    
	    printf_chat("%s's trust status: %s", c.name.c, v?"True":"False");

	    char buf[NB_SLEN];
	    saprintf(buf, "Your trust status was changed to: %s", v?"True":"False");
	    post_chat(1, i, 0, buf, strlen(buf));
	    return;
	}
	if (mode == 1 && ucount == 0) break;
	if (mode == 1 && ucount > 1) {
	    printf_chat("%d online players match \"%s\"", ucount, str1);
	    return;
	}
    }
    printf_chat("&WCannot find a player \"%s\"", str1);
}
