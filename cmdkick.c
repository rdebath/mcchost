
#include "cmdkick.h"

#if INTERFACE
#define CMD_KICK \
    {N"kick", &cmd_kick}
#endif

/*HELP kick H_CMD
&T/Kick [player] <reason>
Kick the player
The player name may be any unique substring
*/

void
cmd_kick(char * cmd, char *arg)
{
    char * str1 = strtok(arg, " ");
    char * str2 = strtok(0, "");

    if (!str1) return cmd_help(0, cmd);

    int uid = find_online_player(str1, 0);
    if (uid < 0) return;

    cmd_payload_t msg = {0};
    saprintf(msg.arg.c, "by %s%s%s", user_id, str2?": &f":"", str2?str2:"");
    msg.cmd_token = cmd_token_kick;
    send_ipc_cmd(1, uid, &msg);
}
