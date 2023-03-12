#include <sys/types.h>
#include <signal.h>

#include "cmdsetvar.h"

#if INTERFACE
#define CMD_SETVAR \
    {N"set", &cmd_setvar, CMD_HELPARG}, {N"setvar", &cmd_setvar, .dup=1}
#endif

/*HELP setvar,set H_CMD
&T/set section name value
Sections are &Tserver&S, &Tlevel&S and &Tblock.&WN&S were
&WN&S is the block definition number

The "level" section is used if you don't specify one of
the known sections.

Any value defined as "seconds" may also be set in minutes,
hours or days by adding "m", "h" or "d"

See &T/help set level&S, &T/help set block&S,
&T/help set server&S and &T/help set system&S
*/

/*HELP set_level
&T/set level &Sname value
Options in "level" include ...
Spawn.X Spawn.Y Spawn.Z Spawn.H Spawn.V
Motd ClickDistance Texture WeatherType SkyColour
CloudColour FogColour AmbientColour SunlightColour
SkyboxColour SideBlock EdgeBlock SideLevel SideOffset
CloudHeight MaxFog CloudsSpeed WeatherSpeed WeatherFade
ExpFog SkyboxHorSpeed SkyboxVerSpeed DisallowChange
ReadOnly NoUnload
*/

/*HELP set_block
&T/set block&S ID name value
&TDefined&S Mostly used to undefine a block (set false).
&TName&S Block name
&TCollide&S Value 0..7
&TTransmitsLight&S Boolean value
&TWalkSound&S Values 0..9
&TFullBright&S Boolean value
&TShape&S Boolean value, true is normal sprite.
&TDraw&S Values 0..7
&TSpeed&S Values 0.25 to 3.95
&TTexture.Top&S, &TTexture.Bottom&S, &TTexture.Left&S,
&TTexture.Right&S, &TTexture.Front&S, &TTexture.Back&S
Textures numbers for the faces of the cube.
For "shape true" blocks use the "right" texture.
&TMin.X&S, &TMin.Y&S, &TMin.Z&S, &TMax.X&S, &TMax.Y&S, &TMax.Z&S
The dimensions of a cubic block (usually 0..16)

&TFallback&S Block number used for downlevel clients.
&TStackBlock&S Block number used for stacking slabs.
&TGrassBlock&S Block number for grass version of this block
&TDirtBlock&S Block number for dirt version of this block
&TOrder&S Inventory position for this block
*/

/*HELP set_server
&T/set server&S name value

&TName&S Server name
&TMotd&S The server level motd
&TMain&S Which level to use as main.
&TSalt&S Secret to use for classicube.net
&TKeyRotation&S Number of seconds between key rotations.
&TMaxPlayers&S Players allowed to connect
&TAFKInterval&S Time til player marked as AFK
&TAFKKickInterval&S Time til player kicked for AFK
&TPlayerUpdateMS&S Minumum period between updates of player positions

&TPrivate&S Set &TTrue&S to hide registration &W**
&THeartbeat&S URL of heartbeat server. &W**
&TPollHeartbeat&S Enable polling of heartbeat server &W**
&TUserSuffix&S Added to end of users on this port &W**
&TDisallowIPAdmin&S Set if Localhost is not an admin &W**
&TDisallowIPVerify&S Set if Localhost must verify &W**
&TAllowPassVerify&S Set to allow password for verification &W**

&W**&S Per port configuration. (&T/help per port&S)
See &T/help betacraft&S to configure for that.
*/

/*HELP per_port
Create an empty file "server.[portno].ini" for each port you wish to configure and start a server for each one. You'll probably want to configure the options with a text editor in each before starting the service.
*/

/*HELP betacraft
The following options need to be set for a Betacraft port.

Heartbeat = http://betacraft.uk/heartbeat.jsp
UseHttpPost = true
Private = true
UserSuffix = +
AllowPassVerify = true

Use &T/set server [varname] [value]&S for each item or alter the server.ini or server.[portno].ini file to include these.
*/

/*HELP set_system
&T/set server&S name value

&TSoftware&S Software name
&TOPFlag&S Okay to send OP flag on client startup
&TNoCPE&S Disable negotiation of CPE
&TLocalnet&S IP address CIDR range of trusted clients
&TSaveInterval&S Time between level save checks
&TBackupInterval&S Time between level backups
&TFlagLogCommands&S Copy user commands to log
&TFlagLogChat&S Copy user chat to log

&Ttcp&S, &Tport&S, &Trunonce&S, &Tinetd&S, &Tdetach&S
  per port defaults for the command line options.
*/


void
cmd_setvar(char * UNUSED(cmd), char * arg)
{
    char * section = strtok(arg, " ");
    char * varname = 0;
    char * value = 0;
    char vbuf[256];

    if (section) {
	if (strcasecmp(section, "server") == 0 ||
		strcasecmp(section, "system") == 0 ||
		strcasecmp(section, "level") == 0 ||
		strncmp(section, "textcolour", 10) == 0) {
	    varname = strtok(0, " ");
	    value = strtok(0, "");
	} else if (strcasecmp(section, "block") == 0) {
	    char * bno = strtok(0, " ");
	    saprintf(vbuf, "block.%s", bno);
	    section = vbuf;
	    varname = strtok(0, " ");
	    value = strtok(0, "");
	} else if (strcasecmp(section, "cuboid") == 0) {
	    char * bno = strtok(0, " ");
	    saprintf(vbuf, "cuboid.%s", bno);
	    section = vbuf;
	    varname = strtok(0, " ");
	    value = strtok(0, "");
	} else {
	    varname = section;
	    section = "level";
	    value = strtok(0, "");
	}
    }
    if (value == 0) value = "";

    if (section == 0 || varname == 0) {
	printf_chat("&SIncomplete section/variable name arguments");
	return;
    }

    ini_state_t stv = {.no_unsafe=1}, *st = &stv;
    st->curr_section = section;

    if (strcasecmp(section, "server") == 0 || strcasecmp(section, "system") == 0 ||
	    strncmp(section, "textcolour", 10) == 0) {

	if (!perm_is_admin()) {
	    printf_chat("&WPermission denied, need to be admin to update system variables");
	    return;
	}

	// Alias
	if (strcasecmp(section, "system") == 0) st->curr_section = "server";

	if (!system_ini_fields(st, varname, &value)) {
	    fprintf_logfile("%s: Setfail %s %s = %s", user_id, section, varname, value);
	    printf_chat("&WOption not available &S[%s&S] %s&S = %s", section, varname, value);
	    return;
	}

	if (ini_settings->use_port_specific_file) {
	    server_ini_tgt = &server_ini_settings;
	    (void)system_x_ini_fields(st, varname, &value);
	    server_ini_tgt = 0;
	}

	save_system_ini_file();

	// Send heartbeat immediatly.
	if (!strcasecmp("private", varname)) {
	    if (alarm_handler_pid)
		kill(alarm_handler_pid, SIGALRM);
	}
    } else {
	if (!perm_level_check(0, 0, 0))
	    return;

	int ro = level_prop->readonly;
	st->looped_read = 1;

	if (level_ini_fields(st, varname, &value)) {
	    if (level_prop->readonly && !ro && current_level_backup_id == 0) {
		printf_chat("&SNote: Will override 'readonly' flag to save flag change");
		level_prop->force_save = 1;
	    }

	    level_prop->dirty_save = 1;
	    level_prop->metadata_generation++;
	    level_prop->last_modified = time(0);
	} else if (current_level_backup_id == 0 && mcc_level_ini_fields(st, varname, &value)) {
	    save_level_ini(current_level_fname);
	} else {
	    fprintf_logfile("%s: Setfailed %s %s = %s", user_id, section, varname, value);
	    printf_chat("&WOption not available &S[%s&S] %s&S = %s&S", section, varname, value);
	    return;
	}

    }

    printf_chat("&SSet %s&S.%s&S=\"%s&S\" ok.", section, varname, value);
}
