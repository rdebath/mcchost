#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include "commands.h"

#if INTERFACE
typedef void (*cmd_func_t)(char * cmd, char * arg);
typedef struct command_t command_t;
struct command_t {
    char * name;
    cmd_func_t function;
    int min_rank;
    int dup;		// Don't show on /cmds (usually a duplicate)
};
#endif

#define N .name=
command_t command_list[] =
{
    CMD_HELP,
    CMD_PLACE,
    CMD_COMMANDS,
    CMD_QUITS,
    CMD_RELOAD,

    {.name = 0}
};
#undef N

// TODO:
// Commands.
// Command expansion/aliases
//      Which way? /cuboid-walls <--> /cuboid walls
//      /os map <--> /map

void
run_command(char * msg)
{
    if (msg[1] == 0) return; //TODO: Repeat command.

    char * cmd = strtok(msg+1, " ");
    if (cmd == 0) return;

    for(int i = 0; command_list[i].name; i++) {
	if (strcasecmp(cmd, command_list[i].name) == 0) {
	    command_list[i].function(cmd, strtok(0, ""));
	    return;
	}
    }

    if (strcasecmp(cmd, "save") == 0)
    {
	char * arg = strtok(0, "");
	if (!client_ipv4_localhost)
	    printf_chat("&WUsage: /save [Auth] filename");
	else if (!arg || !*arg)
	    printf_chat("&WUsage: /save filename");
	else {
	    char buf[256];
	    snprintf(buf, sizeof(buf), "backup/%.20s.1.cw", arg);
	    save_map_to_file(buf);
	    snprintf(buf, sizeof(buf), "backup/%.20s.1.ini", arg);
	    save_ini_file(level_ini_fields, buf);
	    printf_chat("&SFile saved");
	}
	return;
    }

    printf_chat("&SUnknown command \"%s\".", cmd);
    return;

}

/*HELP quit
&T/quit [Reason]
Logout and leave the server
Aliases: /rq
*/
/*HELP rq H_CMD
&T/rq
Logout with RAGEQUIT!!
*/
/*HELP crash H_CMD
&T/crash
Crash the server &T/crash 666&S really do it!
*/
#if INTERFACE
#define CMD_QUITS  {N"quit", &cmd_quit}, {N"rq", &cmd_quit}, \
    {N"hax", &cmd_quit, .dup=1}, {N"hacks", &cmd_quit, .dup=1}, \
    {N"crash", &cmd_quit, .dup=1}, {N"servercrash", &cmd_quit, .dup=1}
#endif
void
cmd_quit(char * cmd, char * arg)
{
    if (strcasecmp(cmd, "rq") == 0) logout("RAGEQUIT!!");

    if (strcasecmp(cmd, "hax") == 0 || strcasecmp(cmd, "hacks") == 0)
	logout("Your IP has been backtraced + reported to FBI Cyber Crimes Unit.");

    if (strcasecmp(cmd, "crash") == 0 || strcasecmp(cmd, "servercrash") == 0) {
	char * crash_type = arg;
	assert(!crash_type || strcmp(crash_type, "666"));
	fatal("Server crash! Error code 42");
    }

    if (arg) {
	char buf[256];
	snprintf(buf, sizeof(buf), "Left the game: %s", arg);
	logout(buf);
    } else
	logout("Left the game.");
}

/*HELP commands,cmds,cmdlist
&T/commands
List all available commands
Aliases: /cmds /cmdlist
*/
#if INTERFACE
#define CMD_COMMANDS  {N"commands", &cmd_commands}, \
    {N"cmds", &cmd_commands, .dup=1}, {N"cmdlist", &cmd_commands, .dup=1}
#endif
void
cmd_commands(UNUSED char * cmd, UNUSED char * arg)
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
*/
#if INTERFACE
#define CMD_RELOAD  {N"reload", &cmd_reload}
#endif
void
cmd_reload(UNUSED char * cmd, char * arg)
{
    if (arg == 0)
	send_map_reload();
    else if (strcasecmp(arg, "all") == 0)
	level_block_queue->generation += 2;
    else
	printf_chat("&WUsage: /reload [all]");
    return;
}
