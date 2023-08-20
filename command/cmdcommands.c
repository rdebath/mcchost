
#include "cmdcommands.h"

/*HELP commands,cmds,cmdlist H_CMD
&T/commands
List all available commands
Aliases: /cmds /cmdlist
*/
#if INTERFACE
#define UCMD_COMMANDS \
    {N"cmds", &cmd_commands}, \
    {N"commands", &cmd_commands, CMD_ALIAS}, \
    {N"cmdlist", &cmd_commands, CMD_ALIAS}
#endif
void
cmd_commands(char * UNUSED(cmd), char * arg)
{
    char buf[BUFSIZ];
    int len = 0;

    int listid = my_user.user_group;
    if (perm_is_admin())
	listid = USER_PERM_ADMIN;
    else if (perm_level_check(0, 0, 1))
	listid = USER_PERM_SUPER;
    if (!arg || !*arg)
	;
    else if (strcasecmp(arg, "all") == 0 || strcasecmp(arg, "admin") == 0)
	listid = USER_PERM_ADMIN;
    else if (strcasecmp(arg, "level") == 0)
	listid = USER_PERM_SUPER;
    else if (strcasecmp(arg, "user") == 0)
	listid = USER_PERM_USER;

    for(int i = 0; command_list[i].name; i++) {
	if (command_list[i].alias)
	    continue;
	if (command_list[i].perm_okay == perm_token_disabled)
	    continue;
	if (command_list[i].perm_okay == perm_token_none)
	    ;
	else if (command_list[i].perm_okay == perm_token_admin &&
	    listid != USER_PERM_ADMIN)
	    continue;
	else if (command_list[i].perm_okay == perm_token_level &&
	    listid == USER_PERM_USER)
	    continue;

	int l = strlen(command_list[i].name);
	if (l + len + 32 < sizeof(buf)) {
	    if (len) {
		strcpy(buf+len, ", ");
		len += 2;
	    } else {
		strcpy(buf+len, "&S");
		len += 2;
	    }
	    strcpy(buf+len, command_list[i].name);
	    len += l;
	}
    }
    printf_chat("&SAvailable commands:");
    printf_chat("%s", buf);
}
