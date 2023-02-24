
#include "cmdskin.h"

#if INTERFACE
#define CMD_SKIN \
    {N"skin", &cmd_skin}
#endif

/*HELP skin H_CMD
&T/Skin [player] skin
[skin] can be:
 - a ClassiCube player's name (e.g Hetal)
 - a Minecraft player's name, if you put a + (e.g +Hypixel)
 - a direct url to a skin
Direct url skins also apply to non human models (e.g pig)
*/

void
cmd_skin(char * cmd, char *arg)
{
    char *str1, *str2;

    if (strcasecmp(cmd, "xskin") == 0) {
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

    char sbuf[256];
    if (str2) {
	if (str2[0] == '+') {
	    saprintf(sbuf, "https://minotar.net/skin/%s.png", str2+1);
	    str2 = sbuf;
	} else if (*str2 && str2[strlen(str2)-1] == '+') {
	    str2[strlen(str2)-1] = 0;
	    saprintf(sbuf, "https://minotar.net/skin/%s.png", str2);
	    str2 = sbuf;
	}
	if (strlen(str2) > MB_STRLEN) {
	    printf_chat("&WThe skin name cannot be longer than %d characters", MB_STRLEN);
	    return;
	}
    }

    if (str1 == 0 || strcmp(str1, "") == 0 || strcasecmp(str1, "-own") == 0) {
	printf_chat("Changed your skin to %s", str2?str2:user_id);
	do_cmd_skin(str2?str2:user_id);
	return;
    }

    int uid = find_online_player(str1, 1, 0);
    if (uid < 0) return;

    client_entry_t c = shdat.client->user[uid];
    if (!c.active) return;

    cmd_payload_t msg = {0};
    if (str2 && strcmp(str2, "-") != 0) saprintf(msg.arg.c, "%s", str2);
    else saprintf(msg.arg.c, "%s", c.name.c);

    printf_chat("@Changed &T%s&S's skin to %s", c.name.c, msg.arg.c);

    msg.cmd_token = cmd_token_changeskin;
    send_ipc_cmd(1, uid, &msg);
}

void
do_cmd_skin(char * newskin)
{
    read_ini_file_fields(&my_user);
    if (newskin && strcmp(newskin, "-") != 0)
	saprintf(my_user.skin, "%s", newskin);
    else strcpy(my_user.skin, user_id);
    write_current_user(3);

    update_player_look();
    reset_player_skinname();
    // Fix current posn as previous resets Spawn and current pos
    send_posn_pkt(-1, 0, player_posn);
}
