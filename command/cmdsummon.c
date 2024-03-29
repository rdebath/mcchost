
#include "cmdsummon.h"

#if INTERFACE
#define UCMD_SUMMON \
    {N"summon", &cmd_summon, CMD_PERM_ADMIN,CMD_HELPARG}, \
    {N"s", &cmd_summon, CMD_ALIAS}
#endif

/*HELP summon H_CMD
&T/Summon [player]
Summon the player here.
*/

void
cmd_summon(char * UNUSED(cmd), char *arg)
{
    char * str1 = strarg(arg);

    int uid = find_online_player(str1, 0, 0);
    if (uid < 0) return;

    int on_level = shdat.client->user[my_user_no].state.on_level;
    if (on_level < 0 || on_level >= MAX_LEVEL || !player_posn.valid || player_on_new_level) {
	printf_chat("&SI can't find you");
	return;
    }
    shdat.client->user[uid].summon_level_id = on_level;
    shdat.client->user[uid].summon_posn = player_posn;
}
