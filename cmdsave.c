#include <sys/types.h>
#include <signal.h>

#include "cmdsave.h"

/*HELP save H_CMD
&T/save
Save the current level and back it up.
*/

#if INTERFACE
#define CMD_SAVELVL {N"save", &cmd_save}
#endif

void
cmd_save(char * UNUSED(cmd), char * UNUSED(arg))
{
    if (my_user_no < 0 || my_user_no >= MAX_USER) return;

    lock_fn(system_lock);
    int my_level = shdat.client->user[my_user_no].on_level;

    if (my_level < 0 || my_level >= MAX_LEVEL || !shdat.client->levels[my_level].loaded) {
	unlock_fn(system_lock);
	printf_chat("&WYou are currently lost in the void, backup failed.");
	return;
    }

    if (shdat.client->levels[my_level].backup_id != 0) {
	unlock_fn(system_lock);
	printf_chat("&WYou are in a museum, it's already backed up");
	return;
    }

    shdat.client->levels[my_level].force_backup = 1;
    level_prop->dirty_save = 1;
    unlock_fn(system_lock);

    if (alarm_handler_pid)
	kill(alarm_handler_pid, SIGALRM);

    printf_chat("&SBacking up level %s", current_level_name);
}
