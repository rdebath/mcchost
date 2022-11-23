
#include "cmdkick.h"

#if INTERFACE
#define CMD_KICK \
    {N"kick", &cmd_kick, CMD_PERM_ADMIN}, \
    {N"ban", &cmd_ban, CMD_PERM_ADMIN}, {N"unban", &cmd_unban, CMD_PERM_ADMIN}
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

    int uid = find_online_player(str1, 0, 0);
    if (uid < 0) return;

    cmd_payload_t msg = {0};
    saprintf(msg.arg.c, "by %s%s%s", user_id, str2?": &f":"", str2?str2:"");
    msg.cmd_token = cmd_token_kick;
    send_ipc_cmd(1, uid, &msg);
}

/*HELP ban,unban H_CMD
&T/Ban [player] <reason>
Kick the player and ban them
The player name may be any unique substring if the player is
online, but if they are offline it must be exact.
Use &T/Unban [player]&S to undo this.
*/

void
cmd_ban(char * cmd, char *arg)
{
    char * str1 = strtok(arg, " ");
    char * str2 = strtok(0, "");

    if (!str1) return cmd_help(0, cmd);

    cmd_payload_t msg = {0};
    saprintf(msg.arg.c, "by %s%s%s", user_id, str2?": &f":"", str2?str2:"");
    msg.cmd_token = cmd_token_ban;

    int uid = find_online_player(str1, 0, 1);
    if (uid >= 0) {
	send_ipc_cmd(1, uid, &msg);
    } else {
	// Set ban data
	userrec_t user_rec;
	if (read_userrec(&user_rec, str1, 1) == 0) {
	    user_rec.ini_dirty = 1;
	    user_rec.user_group = -1;
	    saprintf(user_rec.ban_message, "Banned %s", msg.arg.c);
	    write_userrec(&user_rec, 1);
	    printf_chat("&7%s &Sbanned %s", user_id, str1);
	} else
	    printf_chat("User '%s' not found, use fullname", str1);
    }
}

void
cmd_unban(char * cmd, char *arg)
{
    char * str1 = strtok(arg, " ");

    if (!str1) return cmd_help(0, cmd);

    // Reset ban data
    userrec_t user_rec;
    if (read_userrec(&user_rec, str1, 1) == 0) {
	if (user_rec.user_group >= 0) {
	    // Don't touch a user who might be logged in.
	    printf_chat("User '%s' is not banned", str1);
	    return;
	}
	user_rec.ini_dirty = 1;
	user_rec.user_group = 1;
	user_rec.ban_message[0] = 0;
	write_userrec(&user_rec, 1);
	printf_chat("&7%s &Sunbanned %s", user_id, str1);
    } else
	printf_chat("User '%s' not found, use fullname", str1);
}
