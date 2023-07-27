
#include "cmdnick.h"

#if INTERFACE
#define UCMD_NICK \
    {N"nick", &cmd_nick}, \
    {N"xnick", &cmd_xnick, CMD_ALIAS}, {N"nick", &cmd_xnick, CMD_ALIAS}
#endif

/*HELP nick,xnick H_CMD
&T/Nick [Player] Nickname
/Nick [player] [nick]
Sets the nick of that player.
  If [nick] is not given, reverts [player]'s nick to their account name.
Shortcuts: &T/xnick&S for &T/Nick -own
*/

void
cmd_xnick(char * UNUSED(cmd), char *arg)
{
    if (arg && *arg)
	printf_chat("Changed your nick to %s", arg);
    else
	printf_chat("Removed your nick");
    do_cmd_nick(arg?arg:"");
}

void
cmd_nick(char * UNUSED(cmd), char *arg)
{
    char * str1 = strarg(arg);
    char * str2 = strarg_rest();

    if (str2 && strlen(str2) > MB_STRLEN/4) {
	printf_chat("&WThe nick name cannot be longer than %d characters", MB_STRLEN/4);
	return;
    }

    if (str2 == 0 && find_online_player(str1, 1, 1) < 0) return cmd_xnick(0, arg);

    int uid = find_online_player(str1, 1, 0);
    if (uid < 0) return;

    client_entry_t c = shdat.client->user[uid];
    if (!c.state.active) return;

    cmd_payload_t msg = {0};
    if (str2 && strcmp(str2, "-") != 0) saprintf(msg.arg.c, "%s", str2);
    else msg.arg.c[0] = 0;

    if (!*msg.arg.c)
	printf_chat("@Removed &T%s&S's nick", c.name.c);
    else
	printf_chat("@Changed &T%s&S's nick to %s", c.name.c, msg.arg.c);

    msg.cmd_token = cmd_token_changenick;
    send_ipc_cmd(1, uid, &msg);
}

void
do_cmd_nick(char * newnick)
{
    read_ini_file_fields(&my_user);
    if (newnick && strcmp(newnick, "-") != 0)
	saprintf(my_user.nick, "%s", newnick);
    else strcpy(my_user.nick, "");
    write_current_user(3);

    update_player_look();
    reset_player_skinname();
    // Fix current posn as previous resets Spawn and current pos
    send_posn_pkt(-1, 0, player_posn);
}
