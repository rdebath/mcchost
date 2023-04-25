
#include "cmdtitle.h"

#if INTERFACE
#define UCMD_TITLE \
    {N"title", &cmd_title}, {N"xtitle", &cmd_title, CMD_ALIAS, .nodup=1 }
#endif

/*HELP title H_CMD
&T/Title [Player] Title
Sets the title of [player]
  If [title] is not given, removes [player]'s title.
Shortcuts: &T/XTitle for /Title -own
*/

void
cmd_title(char * cmd, char *arg)
{
    char *str1, *str2;

    if (strcasecmp(cmd, "xtitle") == 0) {
	str1 = "-own";
	str2 = strtok(arg, "");
    } else {
	if (arg) {
	    str1 = strtok(arg, " ");
	    str2 = strtok(0, "");
	} else str1=str2=0;
	if (str1 && strcasecmp(str1, "-own") == 0)
	    ;
	else if (str2 == 0) {str2 = str1; str1 = 0;}
    }

    if (str2 && strlen(str2) > MB_STRLEN/4) {
	printf_chat("&WThe title cannot be longer than %d characters", MB_STRLEN/4);
	return;
    }

    if (str1 == 0 || strcmp(str1, "") == 0 || strcasecmp(str1, "-own") == 0) {
	if (str2)
	    printf_chat("Changed your title to %s", str2);
	else
	    printf_chat("Removed your title");
	do_cmd_title(str2?str2:"");
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
