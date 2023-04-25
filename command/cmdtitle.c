
#include "cmdtitle.h"

#if INTERFACE
#define UCMD_TITLE \
    {N"title", &cmd_title}, \
    {N"xtitle", &cmd_xtitle, CMD_ALIAS}, {N"title-own", &cmd_xtitle, CMD_ALIAS}
#endif

/*HELP title,xtitle H_CMD
&T/Title [Player] Title
Sets the title of [player]
  If [title] is not given, removes [player]'s title.
Shortcuts: &T/XTitle for /Title -own
*/

void
cmd_xtitle(char * UNUSED(cmd), char *arg)
{
    if (arg && *arg)
	printf_chat("Changed your title to %s", arg);
    else
	printf_chat("Removed your title");
    do_cmd_title(arg?arg:"");
}

void
cmd_title(char * UNUSED(cmd), char *arg)
{
    char * str1 = strarg(arg);
    char * str2 = strarg_rest();

    if (str2 && strlen(str2) > MB_STRLEN/4) {
	printf_chat("&WThe title cannot be longer than %d characters", MB_STRLEN/4);
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
	printf_chat("@Removed &T%s&S's title", c.name.c);
    else
	printf_chat("@Changed &T%s&S's title to %s", c.name.c, msg.arg.c);

    msg.cmd_token = cmd_token_changetitle;
    send_ipc_cmd(1, uid, &msg);
}

void
do_cmd_title(char * newtitle)
{
    read_ini_file_fields(&my_user);
    if (newtitle && strcmp(newtitle, "-") != 0)
	saprintf(my_user.title, "%s", newtitle);
    else strcpy(my_user.title, "");
    write_current_user(3);

    update_player_look();
    reset_player_skinname();
    // Fix current posn as previous resets Spawn and current pos
    send_posn_pkt(-1, 0, player_posn);
}
