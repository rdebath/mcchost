#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <signal.h>

#include "commands.h"

#if INTERFACE
typedef void (*cmd_func_t)(char * cmd, char * arg);
typedef struct command_t command_t;
struct command_t {
    char * name;
    cmd_func_t function;
    int min_rank;
    int dup;		// Don't show on /cmds (usually a duplicate)
    int nodup;		// No a dup, don't use previous.
};
#endif

// TODO:
// Commands.
// Command expansion/aliases
//   Which way?
//      /cuboid-walls <-- /cuboid walls
//        -- perms can be done on each subcommand
//      /os map --> /map
//        -- os is a grouper like "cuboid" above with some short subcmds

void
run_command(char * msg)
{
    if (msg[1] == 0) return; //TODO: Repeat command.

    char * cmd = strtok(msg+1, " ");
    if (cmd == 0) return;
    if (strcasecmp(cmd, "womid") == 0) { player_last_move = time(0); return; }
    if (!user_authenticated) {
	if (strcasecmp(cmd, "pass") != 0 && strcasecmp(cmd, "setpass") != 0 &&
	    strcasecmp(cmd, "quit") != 0 && strcasecmp(cmd, "rq") != 0)
	{
	    printf_chat("You must verify using &T/pass [pass]&S first (or &T/rq&S)");
	    return;
	}
    }

    for(int i = 0; command_list[i].name; i++) {
	if (strcasecmp(cmd, command_list[i].name) == 0) {
	    int c = i;
	    while(c>0 && command_list[c].dup && !command_list[c].nodup &&
		command_list[c].function == command_list[c-1].function)
		c--;

	    cmd = command_list[c].name;
	    char * arg = strtok(0, "");
	    if (server->flag_log_commands)
		fprintf_logfile("%s used /%s%s%s", user_id, cmd, arg?" ":"",arg?arg:"");
	    update_player_move_time();
	    command_list[c].function(cmd, arg);
	    return;
	}
    }

    printf_chat("&SUnknown command \"%s&S\" -- see &T/cmds", cmd);
    return;
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
&T/hax [Reason]
Perform various server hacks, OPERATORS ONLY!
Aliases: /hacks
*/
#if INTERFACE
#define CMD_QUITS  {N"quit", &cmd_quit}, {N"rq", &cmd_quit}, \
    {N"hax", &cmd_quit}, {N"hacks", &cmd_quit, .dup=1}
#endif

void
cmd_quit(char * cmd, char * arg)
{
    if (strcasecmp(cmd, "rq") == 0) logout("RAGEQUIT!!");

    if (strcasecmp(cmd, "hax") == 0 || strcasecmp(cmd, "hacks") == 0)
	logout("Your IP has been backtraced + reported to FBI Cyber Crimes Unit.");

    if (arg) {
	char buf[256];
	saprintf(buf, "Left the game: %s", arg);
	logout(buf);
    } else
	logout("Left the game.");
}

/*HELP commands,cmds,cmdlist H_CMD
&T/commands
List all available commands
Aliases: /cmds /cmdlist
*/
#if INTERFACE
#define CMD_COMMANDS  {N"commands", &cmd_commands}, \
    {N"cmds", &cmd_commands, .dup=1}, {N"cmdlist", &cmd_commands, .dup=1}
#endif
void
cmd_commands(char * UNUSED(cmd), char * UNUSED(arg))
{
    char buf[BUFSIZ];
    int len = 0;

    for(int i = 0; command_list[i].name; i++) {
	if (command_list[i].dup)
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

/*HELP reload H_CMD
&T/reload [all]
&T/reload&S -- Reloads your session
&T/reload all&S -- Reloads everybody on this level
&T/reload classic&S -- Reloads your session in classic mode.
*/
#if INTERFACE
#define CMD_RELOAD  {N"reload", &cmd_reload}
#endif
void
cmd_reload(char * UNUSED(cmd), char * arg)
{
    if (arg == 0) {
	if (classic_limit_blocks) {
	    level_block_limit = client_block_limit;
	    classic_limit_blocks = 0;
	}
	check_block_queue(1);
	send_map_reload();
    } else if (strcasecmp(arg, "classic") == 0 && extn_blockdefn && customblock_enabled) {
	level_block_limit = Block_CP;
	classic_limit_blocks = 1;
	send_map_reload();
    } else if (strcasecmp(arg, "all") == 0) {
	level_block_queue->generation += 2;
    } else
	printf_chat("&WUsage: /reload [all]");
    return;
}
