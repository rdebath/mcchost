
#include "cmdafk.h"

/*HELP afk H_CMD
&T/Afk message
Mark yourself AFK with a message
*/

#if INTERFACE
#define UCMD_AFK \
    {N"afk", &cmd_afk}
#endif

void
cmd_afk(char * UNUSED(cmd), char * arg)
{
    struct timeval now;
    gettimeofday(&now, 0);
    if (!shdat.client) return;

    if (!player_is_afk) {
	player_is_afk = 1;
	shdat.client->user[my_user_no].state.is_afk = 1;
	printf_chat("@&S-%s&S- &Sis AFK %s", player_list_name.c, arg?arg:"");
    } else
	update_player_move_time();
}
