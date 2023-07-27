
#include "cmdskin.h"

#if INTERFACE
#define UCMD_SKIN \
    {N"skin", &cmd_skin}, \
    {N"xskin", &cmd_xskin, CMD_ALIAS}, {N"skin-own", &cmd_xskin, CMD_ALIAS}
#endif

/*HELP skin,xskin H_CMD
&T/Skin [player] skin
[skin] can be:
 - a ClassiCube player's name (e.g Hetal)
 - a Minecraft player's name, if you put a + (e.g +Hypixel)
 - a direct url to a skin
Direct url skins also apply to non human models (e.g pig)
*/

void
cmd_xskin(char * UNUSED(cmd), char *arg)
{
    char sbuf[256];
    if (arg) {
	int v = skin_trans(sbuf, arg);
	if (v<0) return;
	if (v==1) arg = sbuf;
    }
    printf_chat("Changed your skin to %s", arg?arg:user_id);
    do_cmd_skin(arg?arg:user_id);
}

int
skin_trans(char * sbuf, char * name)
{
    if (strlen(name) > MB_STRLEN) {
	printf_chat("&WThe skin name cannot be longer than %d characters", MB_STRLEN);
	return -1;
    }
    if (name[0] == '+') {
	sprintf(sbuf, "https://minotar.net/skin/%s.png", name+1);
	return 1;
    } else if (*name && name[strlen(name)-1] == '+') {
	name[strlen(name)-1] = 0;
	sprintf(sbuf, "https://minotar.net/skin/%s.png", name);
	return 1;
    }
    return 0;
}

void
cmd_skin(char * UNUSED(cmd), char *arg)
{
    char * str1 = strarg(arg);
    char * str2 = strarg_rest();

    char sbuf[256];
    if (str2) {
	int v = skin_trans(sbuf, str2);
	if (v<0) return;
	if (v==1) str2 = sbuf;
    }

    if (str2 == 0 && find_online_player(str1, 1, 1) < 0) return cmd_xskin(0, arg);

    int uid = find_online_player(str1, 1, 0);
    if (uid < 0) return;

    client_entry_t c = shdat.client->user[uid];
    if (!c.state.active) return;

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
