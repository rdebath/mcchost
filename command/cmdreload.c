
#include "cmdreload.h"

/*HELP reload,rd,rejoin H_CMD
&T/reload [all]
&T/reload&S -- Reloads your session
&T/reload all&S -- Reloads everybody on this level
&T/reload classic&S -- Reloads your session in classic mode.
*/
#if INTERFACE
#define UCMD_RELOAD {N"reload", &cmd_reload}, \
    {N"rd", &cmd_reload, CMD_ALIAS}, {N"rejoin", &cmd_reload, CMD_ALIAS}
#endif
void
cmd_reload(char * UNUSED(cmd), char * arg)
{
    if (*arg == 0) {
	if (classic_limit_blocks) {
	    level_block_limit = client_block_limit;
	    classic_limit_blocks = 0;
	}
	check_block_queue(1);
	send_map_reload();
    } else if (strcasecmp(arg, "classic") == 0) {
	if (extn_blockdefn && customblock_enabled) {
	    level_block_limit = Block_CP;
	    classic_limit_blocks = 1;
	    send_map_reload();
	} else
	    printf_chat("&WYour client is already running in classic mode.");
    } else if (strcasecmp(arg, "all") == 0) {
	level_block_queue->generation += 2;
    } else
	printf_chat("&WUsage: /reload [all]");
    return;
}
