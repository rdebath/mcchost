
#include "cmdnick.h"

#if INTERFACE
#define CMD_NICK \
    {N"nick", &cmd_nick}, {N"xnick", &cmd_nick, .dup=1, .nodup=1 }
#endif

/*HELP nick H_CMD
&T/Nick [Player] Nickname
/Nick [player] [nick]
Sets the nick of that player.
  If [nick] is not given, reverts [player]'s nick to their account name.
Shortcuts: &T/xnick&S for &T/Nick -own
*/

void
cmd_nick(char * cmd, char *arg)
{
    char *str1, *str2;

    if (strcasecmp(cmd, "xnick") == 0) {
	str1 = "-own";
	str2 = strtok(arg, "");
    } else {
	str1 = strtok(arg, " ");
	str2 = strtok(0, "");
	if (str1 && strcasecmp(str1, "-own") == 0)
	    ;
	else if (str2 == 0) {str2 = str1; str1 = 0;}
    }

    if (str2 && strlen(str2) > MB_STRLEN/4) {
	printf_chat("&WThe nick name cannot be longer than %d characters", MB_STRLEN/4);
	return;
    }

    if (str1 == 0 || strcmp(str1, "") == 0 || strcasecmp(str1, "-own") == 0) {
	if (str2)
	    printf_chat("Changed your nick to %s", str2);
	else
	    printf_chat("Removed your nick");
	do_cmd_nick(str2?str2:"");
	return;
    }

    int uid = find_online_player(str1, 1, 0);
    if (uid < 0) return;

    client_entry_t c = shdat.client->user[uid];
    if (!c.active) return;

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
