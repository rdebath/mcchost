
#include "cmdcmdset.h"

/*HELP cmdset H_CMD
&T/cmdset command permission_group
*/

#if INTERFACE
#define UCMD_CMDSET  {N"cmdset", &cmd_cmdset, CMD_PERM_ADMIN,CMD_HELPARG}
#endif

void
cmd_cmdset(char * UNUSED(cmd), char * arg)
{
    char * str1 = strarg(arg);
    char * str2 = strarg_rest();

    int group = INT_MIN;
    for(int i = 0; i<CMD_PERM_CNT; i++) {
        if (str2 && strcasecmp(cmd_perms[i], str2) == 0) {
            group = i;
            break;
        }
    }
    if (group < 0) {
        char pbuf[256];
        sprintf(pbuf, "&WPermission '%s' is unknown, valid are: ", str2?str2:"");
        for(int i = 0; i<CMD_PERM_CNT; i++) {
            if(i) strcat(pbuf, ", ");
            strcat(pbuf, cmd_perms[i]);
        }
        printf_chat("%s", pbuf);
        return;
    }

    for(int i = 0; command_list[i].name; i++) {
        if (strcasecmp(str1, command_list[i].name) != 0) continue;

        int c = i;
        while(c>0 && !command_list[c].nodup && command_list[c].function == command_list[c-1].function)
            c--;

	command_list[c].perm_okay = group;
	save_cmdset();
	printf_chat("Command '%s' set to group '%s'", command_list[c].name, cmd_perms[group]);
	return;
    }

    printf_chat("&WCommand '%s' not found", str1);
}


void
save_cmdset()
{
    save_ini_file(cmdset_ini_fields, CMDSET_CONF_NAME);
    cmd_payload_t msg = {0};
    saprintf(msg.arg.c, "Permission update");
    msg.cmd_token = cmd_token_loadperm;

    send_ipc_cmd(0, 0, &msg);
}
