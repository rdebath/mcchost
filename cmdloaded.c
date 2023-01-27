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
// Showing levels 1-3 (out of 3)
// Use &T/Maps&S for all levels.

    for(int lvid=0; lvid<MAX_LEVEL; lvid++) {
	if (!shdat.client->levels[lvid].loaded) continue;
	nbtstr_t lv = shdat.client->levels[lvid].level;
	int bid = shdat.client->levels[lvid].backup_id;
	if (bid == 0)
	    printf_chat("&SMap '%s' is loaded", lv.c);
	else
	    printf_chat("&SMuseum %d of '%s' is loaded", bid, lv.c);
    }

    unlock_fn(system_lock);
}
