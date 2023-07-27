
#include "find_player.h"

int
find_online_player(char * user_txt, int allow_self, int quiet)
{
    int ucount = 0;
    if (!user_txt || !*user_txt) return -1;
    for(int mode = 0; mode < 3; mode++) {
	for(int i = 0; i < MAX_USER; i++)
	{
	    if (!allow_self && i == my_user_no) continue; // Me
	    client_entry_t c = shdat.client->user[i];

	    if (!c.state.active) continue;

	    if (mode == 0) {
		// Exact match
		if (strcasecmp(user_txt, c.name.c) != 0) continue; // Not them.
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
		printf_chat("&WCan't identify user \"%s\" the pattern matches %d users", user_txt, ucount);
	    return -1;
	}
    }
    if (!quiet)
	printf_chat("&WCannot find user %s", user_txt);
    return -1;
}

int
find_player(char * partname, char * namebuf, int l, int allow_self, int quiet)
{
    int ucount = 0;
    int acount = 0;
    int uid = -1;
    int skipped = 0;
    char saved_name[NB_SLEN];

    for(int i = 0; i < MAX_USER; i++)
    {
	if (!allow_self && i == my_user_no) continue; // Me
	client_entry_t c = shdat.client->user[i];
	if (!c.state.active) continue;

	// Exact match
	if (strcasecmp(partname, c.name.c) == 0) {
	    snprintf(namebuf, l, "%s", c.name.c);
	    return i;
	}

	if (my_strcasestr(c.name.c, partname) != 0) {
	    if (uid == -1) uid = i;
	    ucount++;

            if (ucount == 1) {
                snprintf(namebuf, l, "%s", c.name.c);
		memcpy(saved_name, c.name.c, sizeof(saved_name));
            } else if (strlen(namebuf) + strlen(c.name.c) + 3 < l) {
                strcat(namebuf, ", ");
                strcat(namebuf, c.name.c);
            } else
		skipped++;
	}
    }

    acount = match_user_name(partname, namebuf, l, 1, &skipped);
    if (!allow_self && acount == 1 && strcasecmp(namebuf, user_id) == 0)
	acount = 0;
    if (acount == 1 || ucount == 1) {
	if (acount != 1) snprintf(namebuf, l, "%s", saved_name);
	if (uid < 0) return MAX_USER; // User not logged in.
	return uid;
    }

    if (acount>1) {
	if (!quiet) {
	    if (skipped)
		printf_chat("The id \"%s\" matches %d users including %s", partname, acount, namebuf);
	    else
		printf_chat("The id \"%s\" matches %d users; %s", partname, acount, namebuf);
	}
	return -acount;
    }

    if (!quiet) printf_chat("&WCannot find user %s", partname);
    return -1;
}
