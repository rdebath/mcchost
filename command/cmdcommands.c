
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

static int
p_cmd_sort_cmp(const void *p1, const void *p2)
{
    // TODO: Do I want "important" commands first?
    command_t ** e1 = (command_t **)p1;
    command_t ** e2 = (command_t **)p2;
    int r = strnatcasecmp((*e1)->name, (*e2)->name);

    return r;
}

void
cmd_commands(char * UNUSED(cmd), char * arg)
{
    char buf[BUFSIZ];
    int len = 0;

    int listid = perm_token_none, list_only = 0;
    if (perm_is_admin())
	listid = perm_token_admin;
    else if (perm_level_check(0, 0, 1))
	listid = perm_token_level;
    if (!arg || !*arg)
	;
    else if (strcasecmp(arg, "all") == 0) {
	listid = perm_token_admin;
    } else if (strcasecmp(arg, "admin") == 0) {
	listid = perm_token_admin;
	list_only = 1;
    } else if (strcasecmp(arg, "level") == 0) {
	listid = perm_token_level;
	list_only = 1;
    } else if (strcasecmp(arg, "user") == 0) {
	listid = perm_token_none;
    } else
	return printf_chat("Lists are: all, admin, level, user");

    int cmd_max = 0;
    for(int i = 0; command_list[i].name; i++)
	cmd_max = i+1;

    command_t ** cmd_sort = calloc(cmd_max, sizeof(command_t*));
    if (!cmd_sort) return;

    int cmd_index = 0;

    // Collect commands
    for(int i = 0; command_list[i].name; i++) {
	if (command_list[i].alias)
	    continue;
	if (command_list[i].perm_okay == perm_token_disabled)
	    continue;
	if (list_only) {
	    if (command_list[i].perm_okay != listid)
		continue;
	} else if (command_list[i].perm_okay == perm_token_none)
	    ;
	else if (command_list[i].perm_okay == perm_token_admin &&
	    listid != perm_token_admin)
	    continue;
	else if (command_list[i].perm_okay == perm_token_level &&
	    listid == perm_token_none)
	    continue;

	cmd_sort[cmd_index++] = command_list+i;
    }

    // sort
    qsort(cmd_sort, cmd_index, sizeof(*cmd_sort), p_cmd_sort_cmp);

    // display
    for(int i = 0; i<cmd_index; i++)
    {
	int l = strlen(cmd_sort[i]->name);
	if (l + len + 32 < sizeof(buf)) {
	    if (len) {
		strcpy(buf+len, ", ");
		len += 2;
	    } else {
		strcpy(buf+len, "&S");
		len += 2;
	    }
	    strcpy(buf+len, cmd_sort[i]->name);
	    len += l;
	}
    }
    printf_chat("&SAvailable commands:");
    printf_chat("%s", buf);

    free(cmd_sort);
}
