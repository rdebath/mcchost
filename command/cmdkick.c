
#include "cmdkick.h"

#if INTERFACE
#define UCMD_KICK \
    {N"kick", &cmd_kick, CMD_PERM_ADMIN,CMD_HELPARG}, \
    {N"ban", &cmd_ban, CMD_PERM_ADMIN,CMD_HELPARG}, \
    {N"unban", &cmd_unban, CMD_PERM_ADMIN,CMD_HELPARG}
#endif

/*HELP kick H_CMD
&T/Kick [player] <reason>
Kick the player
The player name may be any unique substring
*/

void
cmd_kick(char * UNUSED(cmd), char *arg)
{
    char * str1 = strarg(arg);
    char * str2 = strarg_rest();

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
cmd_ban(char * UNUSED(cmd), char *arg)
{
    char * str1 = strarg(arg);
    char * str2 = strarg_rest();

    cmd_payload_t msg = {0};
    saprintf(msg.arg.c, "by %s%s%s", user_id, str2?": &f":"", str2?str2:"");
    msg.cmd_token = cmd_token_ban;

    char user_name[128];
    int uid = find_player(str1, user_name, sizeof(user_name), 0, 0);
    if (uid<0) return;

    if (uid>=0 && uid != MAX_USER) {
	send_ipc_cmd(1, uid, &msg);
    } else {
	// Set ban data
	userrec_t user_rec;
	if (read_userrec(&user_rec, user_name, 1) == 0) {
	    user_rec.ini_dirty = 1;
	    user_rec.user_group = -1;
	    saprintf(user_rec.ban_message, "Banned %s", msg.arg.c);
	    write_userrec(&user_rec, 1);
	    printf_chat("&7%s &Sbanned %s", user_id, user_name);
	} else
	    printf_chat("User detail for '%s' not found", user_name);
    }
}

void
cmd_unban(char * UNUSED(cmd), char *arg)
{
    char * str1 = strarg(arg);
    userrec_t user_rec;

    char user_name[128];
    saprintf(user_name, "%s", str1);
    if (read_userrec(&user_rec, user_name, 1) != 0) {
	int found = match_user_name(str1, user_name, sizeof(user_name), 0, 0);
	if (found != 1) return;

	if (read_userrec(&user_rec, user_name, 1) != 0) {
	    printf_chat("User detail for '%s' not found", user_name);
	    return;
	}
    }

    // Reset ban data
    if (user_rec.user_group >= 0) {
	// Don't touch a user who might be logged in.
	printf_chat("User '%s' is not banned", user_name);
	return;
    }
    user_rec.ini_dirty = 1;
    user_rec.user_group = 1;
    user_rec.ban_message[0] = 0;
    write_userrec(&user_rec, 1);
    printf_chat("&7%s &Sunbanned %s", user_id, user_name);
}
