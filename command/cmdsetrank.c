
#include "cmdsetrank.h"

#if INTERFACE
#define CMD_SETRANK \
    {N"setrank", &cmd_setrank, CMD_PERM_ADMIN,CMD_HELPARG}, {N"rank", &cmd_setrank, .dup=1}
#endif

/*HELP setrank,rank H_CMD
&T/Setrank [player] [rank]
*/

#if INTERFACE
#define USER_PERM_CNT 2
#endif
char * user_perms[USER_PERM_CNT] = { "admin", "user" };

void
cmd_setrank(char * UNUSED(cmd), char *arg)
{
    char * str1 = strtok(arg, " ");
    char * str2 = strtok(0, " ");

    cmd_payload_t msg = {0};
    msg.cmd_token = cmd_token_loaduser;

    int group = INT_MIN;
    for(int i = 0; i<USER_PERM_CNT; i++) {
	if (str2 && strcasecmp(user_perms[i], str2) == 0) {
	    group = i;
	    break;
	}
    }
    if (group < 0) {
	char pbuf[256];
	sprintf(pbuf, "&WPermission '%s' is unknown, valid are: ", str2?str2:"");
	for(int i = 0; i<USER_PERM_CNT; i++) {
	    if(i) strcat(pbuf, ", ");
	    strcat(pbuf, user_perms[i]);
	}
	printf_chat("%s", pbuf);
	return;
    }

    char user_name[128];
    int uid = find_player(str1, user_name, sizeof(user_name), 1, 0);
    if (uid<0) return;

    userrec_t user_rec;
    if (read_userrec(&user_rec, user_name, 1) == 0) {
	user_rec.ini_dirty = 1;
	user_rec.user_group = group;
	user_rec.ban_message[0] = 0;
	write_userrec(&user_rec, 1);
	printf_chat("%s&S set rank of %s to %s", player_list_name.c, user_name, user_perms[group]);
	saprintf(msg.arg.c, "&SYour rank has been set to %s", user_perms[group]);
    } else {
	printf_chat("User detail for '%s' not found", user_name);
	return;
    }

    if (uid >= 0 && uid < MAX_USER)
	send_ipc_cmd(1, uid, &msg);
}
