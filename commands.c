
#include "commands.h"

#if INTERFACE
enum perm_token { perm_token_none, perm_token_admin, perm_token_level, perm_token_disabled };
#define CMD_PERM_CNT 4

#define CMD_PERM_ADMIN  .perm_def=perm_token_admin		/* System admin */
#define CMD_PERM_LEVEL  .perm_def=perm_token_level		/* Level owner */
#define CMD_HELPARG	.help_if_no_args=1
#define CMD_ALIAS	.alias=1				/* Hide from /cmds */

typedef void (*cmd_func_t)(char * cmd, char * arg);
typedef struct command_t command_t;
struct command_t {
    char * name;
    cmd_func_t function;
    enum perm_token perm_okay;
    enum perm_token perm_def;
    uint8_t perm_set;
    uint8_t alias;		// Don't show on /cmds (usually a duplicate)
    uint8_t nodup;		// This is a subcommand, not a dup because of "function"
    uint8_t help_if_no_args;
    uint8_t map;		// Error if no map
};

typedef struct command_limit_t command_limit_t;
struct command_limit_t {
    int max_user_blocks;
    int other_cmd_disabled;
};

#define USER_PERM_ADMIN	0
#define USER_PERM_USER	1
#define USER_PERM_SUPER	2
#define USER_PERM_CNT	3
#endif

command_limit_t command_limits = {.max_user_blocks=400};

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
	printf_chat("&SUnknown command%s", has_command("cmds")?"; see &T/cmds":"");
	return;
    }
    memcpy(cmd, msg+1, l);
    cmd[l] = 0;
    cmd2[0] = 0;

    for(unsigned char * c = cmd; *c; c++)
	if (isupper(*c))
	    *c = tolower(*c);

    char * arg = msg+1+l, *arg2 = "";
    while (*arg == ' ') arg++;

    int al = strcspn(arg, " ");
    if (al + l + 2 < NB_SLEN) {
	char * a = arg;
	int al2 = al;
	if (*a == '-') {al2--; a++;}
	if (al2 <= 0)
	    saprintf(cmd2, "%s-(none)", cmd);
	else
	    saprintf(cmd2, "%s-%.*s", cmd, al2, a);

	for(unsigned char * c = cmd2; *c; c++)
	    if (isupper(*c))
		*c = tolower(*c);

	arg2 = arg+al;
	while (*arg2 == ' ') arg2++;
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
    uint8_t help_if_no_args = 1;
    char * ncmd = 0;
    for(int i = 0; command_list[i].name; i++) {
	if (cmd[0] != command_list[i].name[0]) continue;
	int is_arg = 0;
	if (strcmp(cmd, command_list[i].name) != 0) {
	    is_arg = 1;
	    if (strcmp(cmd2, command_list[i].name) != 0) continue;
	}

	int c = i;
	while(c>0 && !command_list[c].nodup &&
	    command_list[c].function == command_list[c-1].function)
	    c--;

	if (cno == -1 || is_arg) {
	    // Official cmd name
	    ncmd = command_list[c].name;
	    help_if_no_args = command_list[c].help_if_no_args;

	    if (command_list[c].perm_okay == perm_token_disabled)
		continue;

	    cno = c;
	    if (is_arg) arg = arg2;
	}
    }

    if (cno == -1) {
	printf_chat("&SUnknown command \"%s&S\"%s", cmd, has_command("cmds")?" -- see &T/cmds":"");
	return;
    }

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
    if (help_if_no_args && *arg == 0)
	cmd_help(0, ncmd);
    else
	command_list[cno].function(ncmd, arg);
    return;
}

void
init_cmdset_perms()
{
    for(int i = 0; command_list[i].name; i++) {
	if(i>0 && !command_list[i].nodup && command_list[i].function == command_list[i-1].function)
	    continue;

	if (!command_list[i].perm_set) {
	    if (command_limits.other_cmd_disabled == 1)
		command_list[i].perm_okay = perm_token_disabled;
	    else
		command_list[i].perm_okay = command_list[i].perm_def;
	    command_list[i].perm_set = 1;
	}
    }
}

int
has_command(char * cmd_name)
{
    for(int i = 0; command_list[i].name; i++) {
	if (command_list[i].name[0] == cmd_name[0]
	    && command_list[i].perm_okay != perm_token_disabled
	    && strcmp(command_list[i].name, cmd_name) == 0)
	    return 1;
    }
    return 0;
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
#define UCMD_QUITS  {N"quit", &cmd_quit}, \
    {N"rq", &cmd_quit, .nodup=1 }, \
    {N"hax", &cmd_quit, CMD_ALIAS, .nodup=1 }, {N"hacks", &cmd_quit, CMD_ALIAS}, \
    {N"crashserver", &cmd_quit, CMD_ALIAS, .nodup=1}, \
    {N"servercrash", &cmd_quit, CMD_ALIAS}
#endif

void
cmd_quit(char * cmd, char * arg)
{
    if (strcasecmp(cmd, "rq") == 0) logout("RAGEQUIT!!");

    if (strcasecmp(cmd, "hax") == 0)
	logout("Your IP has been backtraced + reported to FBI Cyber Crimes Unit.");

    if (strcasecmp(cmd, "crashserver") == 0)
	logout("Server crash! Error code 0xD482776B");

    if (arg&&*arg)
	logout_f("Left the game: %s", arg);
    else
	logout("Left the game.");
}
