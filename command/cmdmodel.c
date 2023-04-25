
#include "cmdmodel.h"

#if INTERFACE
#define UCMD_MODEL \
    {N"model", &cmd_model}, {N"xmodel", &cmd_model, CMD_ALIAS, .nodup=1}
#endif

/*HELP model H_CMD
&T/Model [player] model
/Model [name] [model] - Sets the model of that player.
Use /Help Model models for a list of models.
Shortcuts: /XModel for /Model -own
*/

/*HELP model_models H_CMD
Available models: Chibi, Chicken, Creeper, Giant, Humanoid, Pig, Sheep, Spider, Skeleton, Zombie, Head, Sit, Corpse
To set a block model, use a block ID for the model name.
Extensions may provide additional models.
For a scaled model, put "|[scale]" after the model name.
  e.g. pig|0.5, chibi|2
*/

void
cmd_model(char * cmd, char *arg)
{
    char *str1, *str2;

    if (strcasecmp(cmd, "xmodel") == 0) {
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

    if (str1 == 0 || strcmp(str1, "") == 0 || strcasecmp(str1, "-own") == 0) {
	printf_chat("Changed your model to a %s", str2?str2:"humanoid");
	do_cmd_model(str2);
	return;
    }

    int uid = find_online_player(str1, 1, 0);
    if (uid < 0) return;

    client_entry_t c = shdat.client->user[uid];
    if (!c.active) return;

    printf_chat("@Changed &T%s&S's model to a %s", c.name.c, str2?str2:"humanoid");
    cmd_payload_t msg = {0};
    if (str2) saprintf(msg.arg.c, "%s", str2);
    msg.cmd_token = cmd_token_changemodel;
    send_ipc_cmd(1, uid, &msg);
}

void
do_cmd_model(char * newmodel)
{
    read_ini_file_fields(&my_user);
    if (newmodel && strcmp(newmodel, "-") != 0)
	saprintf(my_user.model, "%s", newmodel);
    else *my_user.model = 0;
    write_current_user(3);

    update_player_look();
    send_changemodel_pkt(-1, my_user.model);
}
