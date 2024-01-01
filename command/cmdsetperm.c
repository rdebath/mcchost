
#include "cmdsetperm.h"

#if INTERFACE
#define UCMD_SETPERM \
    {N"setperm", &cmd_setperm, CMD_HELPARG, CMD_PERM_LEVEL}
#endif

/*HELP setperm H_CMD
&T/Setperm [rank] [player]
Rank "admin" access to everything
Rank "user" normal or guest access
Rank "super" owner access to all levels
Use "add" to add the user to "owners" for current level
Use "del" to remove the user from list
*/

void
cmd_setperm(char * UNUSED(cmd), char *arg)
{
    char * rank = strarg(arg);
    char * user = strarg(0);

    cmd_payload_t msg = {0};
    msg.cmd_token = cmd_token_loaduser;

    int group = INT_MIN;
    int perm_type = -1;
    for(int i = 0; i<USER_PERM_CNT; i++) {
	if (rank && strcasecmp(user_perms[i], rank) == 0) {
	    group = i;
	    perm_type = 1;
	    break;
	}
    }

    if (strcasecmp("add", rank) == 0) perm_type = 2;
    if (strcasecmp("del", rank) == 0) perm_type = 3;

    if (perm_type < 0) {
	char pbuf[256];
	sprintf(pbuf, "&WPermission '%s' is unknown, valid are: ", rank?rank:"");
	for(int i = 0; i<USER_PERM_CNT; i++) {
	    if(i) strcat(pbuf, ", ");
	    strcat(pbuf, user_perms[i]);
	}
	printf_chat("%s", pbuf);
	return;
    }

    char user_name[128];
    int uid = find_player(user, user_name, sizeof(user_name), 1, 0);
    if (uid<0) return;

    if (perm_type != 1 && current_level_backup_id > 0) {
	printf_chat("&WPermissions cannot be applied to individual backup levels");
	return;
    }

    if (perm_type == 1 && !perm_is_admin()) {
	printf_chat("&WPermission denied, only admin can change that permission");
	return;
    }

    if ((perm_type == 2 || perm_type == 3) && !perm_level_check(0,0,0))
	return;

    if (perm_type == 1) {
	userrec_t user_rec = {.saveable=1};
	if (read_userrec(&user_rec, user_name, 1) == 0) {
	    user_rec.ini_dirty = 1;
	    user_rec.user_group = group;
	    user_rec.ban_message[0] = 0;
	    write_userrec(&user_rec, 1);
	    printf_chat("%s&S set rank of %s to %s", player_list_name.c, user_name, user_perms[group]);
	    saprintf(msg.arg.c, "&SYour rank has been set to %s", user_perms[group]);

	    if (uid >= 0 && uid < MAX_USER)
		send_ipc_cmd(1, uid, &msg);
	} else
	    printf_chat("User detail for '%s' not found", user_name);
	clear_userrec(&user_rec);
	return;
    }

    if (perm_type == 2) {
	int r = add_strlist(level_prop->op_user_list, sizeof(level_prop->op_user_list), user_name);
	if (r == 1) {
	    printf_chat("User '%s' is in level owner list", user_name);
	    save_level_ini(current_level_name);
	} else if (r == 0)
	    printf_chat("User '%s' is already in the level owner list", user_name);
	else
	    printf_chat("User '%s' cannot be added to level owner list", user_name);
    }
    if (perm_type == 3) {
	if (!del_strlist(level_prop->op_user_list, user_name))
	    printf_chat("User '%s' is not in level owner list", user_name);
	else {
	    printf_chat("User '%s' has been removed from level owner list", user_name);
	    save_level_ini(current_level_name);
	}
    }
}
