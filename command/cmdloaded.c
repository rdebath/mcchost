#include <signal.h>

#include "cmdunload.h"

/*HELP loaded H_CMD
&T/loaded
Show all loaded (unpacked) levels
*/

#if INTERFACE
#define CMD_LOADED {N"loaded", &cmd_loaded}
#endif

void
cmd_loaded(char * UNUSED(cmd), char * UNUSED(arg))
{
    lock_fn(system_lock);

// Loaded levels [physics level] (&W[no]&S if not visitable):
// main [0], classic [1], test [5]

    char line_buf[NB_SLEN] = {0};
    int c = 0;

    printf_chat("&SLoaded levels:");
    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	nbtstr_t lv = shdat.client->levels[lvid].level;
	int bid = shdat.client->levels[lvid].backup_id;
	char s[NB_SLEN];
	if (bid == 0)
	    saprintf(s, "%s", lv.c);
	else
	    saprintf(s, "%s/%d", lv.c, bid);

	int l = strlen(s);
        if (c != 0 && c + 2 + l > NB_SLEN-1-3) {
            printf_chat("&7%s,", line_buf);
            c = 0; *line_buf = 0;
        }
        if (c) {
            strcpy(line_buf+c, ", ");
            c += 2;
        }
	strcpy(line_buf+c, s);
	c += l;
    }
    if (c)
        printf_chat("&7%s", line_buf);

    unlock_fn(system_lock);
}
