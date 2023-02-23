
#include "cmdsummon.h"

#if INTERFACE
#define CMD_SUMMON \
    {N"summon", &cmd_summon, CMD_PERM_ADMIN,CMD_HELPARG}, \
    {N"s", &cmd_ban, .dup=1}
#endif

/*HELP summon H_CMD
&T/Summon [player]
Summon the player here.
*/

void
cmd_summon(char * UNUSED(cmd), char *arg)
{
    char * str1 = strtok(arg, " ");
    // char * str2 = strtok(0, "");

    int uid = find_online_player(str1, 0, 0);
    if (uid < 0) return;

    int on_level = shdat.client->user[my_user_no].on_level;
    if (on_level < 0 || on_level >= MAX_LEVEL) {
	printf_chat("&SSummoning into the void doesn't work");
	return;
    }
    if (!player_posn.valid || player_on_new_level) {
	printf_chat("&SI can't find you");
	return;
    }
    shdat.client->user[uid].summon_level_id = on_level;
    shdat.client->user[uid].summon_posn = player_posn;
}
