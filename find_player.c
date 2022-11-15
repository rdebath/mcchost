
#include "find_player.h"

int
find_online_player(char * user_txt, int allow_self, int quiet)
{
    int ucount = 0;
    for(int mode = 0; mode < 3; mode++) {
	for(int i = 0; i < MAX_USER; i++)
	{
	    if (!allow_self && i == my_user_no) continue; // Me
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
	    if (!quiet)
		printf_chat("&WCan't identify user \"%s\" that matches %d users", user_txt, ucount);
	    return -1;
	}
    }
    if (!quiet)
	printf_chat("&WCannot find user %s", user_txt);
    return -1;
}
