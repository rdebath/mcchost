
#include "commands.h"

#if INTERFACE
enum perm_token { perm_token_none, perm_token_admin, perm_token_level, perm_token_disabled };
#define CMD_PERM_CNT 4

#define CMD_PERM_ADMIN  .perm_def=perm_token_admin		/* System admin */
#define CMD_PERM_LEVEL  .perm_def=perm_token_level		/* Level owner */
#define CMD_HELPARG	.help_if_no_args=1

typedef void (*cmd_func_t)(char * cmd, char * arg);
typedef struct command_t command_t;
struct command_t {
    char * name;
    cmd_func_t function;
    enum perm_token perm_okay;
    enum perm_token perm_def;
    int dup;		// Don't show on /cmds (usually a duplicate)
    int nodup;		// No a dup, don't use previous.
    int help_if_no_args;
    int map;		// Error if no map
};

typedef struct command_limit_t command_limit_t;
struct command_limit_t {
    int max_user_blocks;
};

#define USER_PERM_ADMIN	0
#define USER_PERM_USER	1
#define USER_PERM_SUPER	2
#define USER_PERM_CNT	3
#endif

command_limit_t command_limits = {400};

char * cmd_perms[CMD_PERM_CNT] = { "user", "admin", "level", "disabled" };

char * user_perms[USER_PERM_CNT] = { "admin", "user", "super" };

void
run_command(char * msg)
{
    if (msg[1] == 0) return; //TODO: Repeat command.

    char cmd[NB_SLEN];
    char cmd2[NB_SLEN];

    int l = strcspn(msg+1, " ");
    if (l>=MB_STRLEN) {
	printf_chat("&SUnknown command; see &T/cmds");
	return;
    }
    memcpy(cmd, msg+1, l);
    cmd[l] = 0;
    cmd2[0] = 0;

    for(unsigned char * c = cmd; *c; c++)
	if (isupper(*c))
	    *c = tolower(*c);

    char * arg = msg+2+l;
    while (*arg == ' ') arg++;

    int al = strcspn(arg, " ");
    if (al + l + 2 < NB_SLEN) {
	char * a = arg;
	int al2 = al;
	if (*a == '-') {al2--; a++;}
	saprintf(cmd2, "%s-%*s", cmd, al2, arg);

	for(unsigned char * c = cmd2; *c; c++)
	    if (isupper(*c))
		*c = tolower(*c);
    }

    if (strcmp(cmd, "womid") == 0) { player_last_move = time(0); return; }
    if (!user_authenticated) {
	if (strcmp(cmd, "pass") != 0 && strcmp(cmd, "setpass") != 0 &&
	    strcmp(cmd, "quit") != 0 && strcmp(cmd, "rq") != 0)
	{
	    printf_chat("You must verify using &T/pass [pass]&S first (or &T/rq&S)");
	    return;
	}
    }

    if (add_antispam_event(player_cmd_spam, server->cmd_spam_count, server->cmd_spam_interval, server->cmd_spam_ban)) {
	int secs = 0;
	if (player_cmd_spam->banned_til)
	    secs = player_cmd_spam->banned_til - time(0);
	if (secs < 2)
	    printf_chat("You have been blocked from using commands");
	else if (player_cmd_spam->ban_flg == 0) {
	    player_cmd_spam->ban_flg = 1;
	    printf_chat("You have been blocked from using commands for %d seconds", secs);
	} else
	    printf_chat("Blocked from using commands for %d seconds", secs);
	return;
    }

    int cno = -1;
    for(int i = 0; command_list[i].name; i++) {
	if (cmd[0] != command_list[i].name[0]) continue;
	int is_arg = 0;
	if (strcmp(cmd, command_list[i].name) != 0) {
	    is_arg = 1;
	    if (strcmp(cmd2, command_list[i].name) != 0) continue;
	}

	int c = i;
	while(c>0 && command_list[c].dup && !command_list[c].nodup &&
	    command_list[c].function == command_list[c-1].function)
	    c--;

	if (command_list[c].perm_okay == perm_token_disabled)
	    continue;

	if (cno == -1 || is_arg)
	    cno = c;
    }

    if (cno == -1) {
	printf_chat("&SUnknown command \"%s&S\" -- see &T/cmds", cmd);
	return;
    }

    char * ncmd = command_list[cno].name;

    if (command_list[cno].perm_okay != perm_token_none && user_authenticated) {
	if (command_list[cno].perm_okay == perm_token_admin) {
	    if (!perm_is_admin()) {
		printf_chat("&WPermission denied, only admin can run /%s", ncmd);
		fprintf_logfile("%s denied cmd /%s%s%s", user_id, ncmd, *arg?" ":"",arg);
		return;
	    }
	} else if (command_list[cno].perm_okay == perm_token_level) {
	    if (!perm_level_check(0,0,0))
		return;
	}
    }

    if (server->flag_log_commands) {
	int redact_args = (!strcmp(ncmd, "pass") ||
			   !strcmp(ncmd, "setpass"));
	if (!server->flag_log_place_commands &&
		strcmp(ncmd, "place") == 0)
	    ;
	else if (redact_args)
	    fprintf_logfile("%s used /%s%s", user_id, ncmd, *arg?" <redacted>":"");
	else
	    fprintf_logfile("%s used /%s%s%s", user_id, ncmd, *arg?" ":"",arg);
    }

    if (strcmp(ncmd, "afk") != 0)
	update_player_move_time();

    if ((!level_prop || !level_blocks) && command_list[cno].map)
	printf_chat("&WCommand failed, map is not loaded");
    else
    if (command_list[cno].help_if_no_args && *arg == 0)
	cmd_help(0, ncmd);
    else
	command_list[cno].function(ncmd, arg);
    return;
}

void
init_cmdset_perms()
{
    for(int i = 0; command_list[i].name; i++) {
	command_list[i].perm_okay = command_list[i].perm_def;
    }
}

/*HELP quit H_CMD
&T/quit [Reason]
Logout and leave the server
Aliases: /rq
*/
/*HELP rq H_CMD
&T/rq
Logout with RAGEQUIT!!
*/
/*HELP hax H_CMD
&T/hax
Perform various server hacks, OPERATORS ONLY!
Aliases: /hacks
*/
#if INTERFACE
#define CMD_QUITS  {N"quit", &cmd_quit}, \
    {N"rq", &cmd_quit, .nodup=1 }, \
    {N"hax", &cmd_quit, .dup=1, .nodup=1 }, {N"hacks", &cmd_quit, .dup=1}
#endif

void
cmd_quit(char * cmd, char * arg)
{
    if (strcasecmp(cmd, "rq") == 0) logout("RAGEQUIT!!");

    if (strcasecmp(cmd, "hax") == 0 || strcasecmp(cmd, "hacks") == 0)
	logout("Your IP has been backtraced + reported to FBI Cyber Crimes Unit.");

    if (arg&&*arg)
	logout_f("Left the game: %s", arg);
    else
	logout("Left the game.");
}

/*HELP commands,cmds,cmdlist H_CMD
&T/commands
List all available commands
Aliases: /cmds /cmdlist
*/
#if INTERFACE
#define CMD_COMMANDS \
    {N"cmds", &cmd_commands}, \
    {N"commands", &cmd_commands, .dup=1}, \
    {N"cmdlist", &cmd_commands, .dup=1}
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
	if (command_list[i].dup)
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
