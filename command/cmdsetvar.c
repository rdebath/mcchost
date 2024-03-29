#include <sys/types.h>
#include <signal.h>

#include "cmdsetvar.h"

#if INTERFACE
#define UCMD_SETVAR \
    {N"setvar", &cmd_setvar, CMD_HELPARG}, {N"set", &cmd_setvar, CMD_ALIAS}
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
&TSalt&S Secret to use for classicube.net (&T/help set salt&S)
&TKeyRotation&S Number of seconds between key rotations.
&TMaxPlayers&S Players allowed to connect
&TAFKInterval&S Time til player marked as AFK
&TAFKKickInterval&S Time til player kicked for AFK
&TPlayerUpdateMS&S Minumum period between updates of player positions

&TPrivate&S Set &TTrue&S to hide registration &W**
&TDisableWebClient&S Disables registration of web client when &TTrue&S &W**
&THeartbeat&S URL of heartbeat server. &W**
&TPollHeartbeat&S Enable polling of heartbeat server &W**
&TUserSuffix&S Added to end of users on this port &W**
&TDisallowIPAdmin&S Set if Localhost is not an admin &W**
&TDisallowIPVerify&S Set if Localhost must verify &W**
&TLocalnet&S A cidr (eg &T192.168.0.0/9&S) of IP addresses treated the same as Localhost.
&TDisableSaltLogin&S Disable MPPass/Salt verification &W**
&TAllowPassVerify&S Allow password for verification &W**
&TUseHttpPost&S Use POST on reg server &W**
&TUseHttp09Arg&S Allow curl to use http0.9&W**
&TUseIPv6&S Use IPv6 on reg server &W**
&TOmitSoftwareVersion&S Do not add version to end of software name &W**

&W**&S Per port configuration. (&T/help per port&S)
See &T/help betacraft&S to configure for that.
*/

/*HELP per_port
Create an empty file "server.[portno].ini" for each port you wish to configure and start a server for each one with the -port option.
You'll probably want to configure the options with a text editor in each before starting the service.
*/

/*HELP betacraft
The following options need to be set for a Betacraft port.

Heartbeat = http://betacraft.uk/heartbeat.jsp
UseHttpPost = true
Private = false
UserSuffix = +
DisableSaltLogin = true
AllowPassVerify = true

Use &T/set server [varname] [value]&S for each item or alter the server.ini or server.[portno].ini file to include these.
*/

/*HELP set_salt
The &TSalt&S and &TRotation&S options in the server section are used to generate the secret sent to the registration server (eg: Classicube.net or Betacraft)
If the &TRotation&S option is set to zero the &TSalt&S is used directly.
When the &TRotation&S is set to a time (eg: &T6h&S) the key sent will be changed that frequently.
If the &TSalt&S is set to "-" mppass validation will be turned off and a dummy key sent to the registration server.
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
    char * section = strarg(arg);
    char * varname = 0;
    char * value = 0;
    char vbuf[256];

    if (section) {
	if (strcasecmp(section, "server") == 0 ||
		strcasecmp(section, "system") == 0 ||
		strcasecmp(section, "level") == 0 ||
		strncmp(section, "textcolour", 10) == 0) {
	    varname = strarg(0);
	    value = strarg_rest();

	    // Alias
	    if (strcasecmp(section, "system") == 0) section = "server";

	} else if (strcasecmp(section, "block") == 0) {
	    char * bno = strarg(0);
	    saprintf(vbuf, "block.%s", bno);
	    section = vbuf;
	    varname = strarg(0);
	    value = strarg_rest();
	} else if (strcasecmp(section, "cuboid") == 0) {
	    char * bno = strarg(0);
	    saprintf(vbuf, "cuboid.%s", bno);
	    section = vbuf;
	    varname = strarg(0);
	    value = strarg_rest();
	} else {
	    varname = section;
	    section = "level";
	    value = strarg_rest();
	}
    }
    if (value == 0) value = "";

    if (section == 0 || varname == 0) {
	printf_chat("&SIncomplete section/variable name arguments");
	return;
    }

    if (setvar(section, varname, value))
	printf_chat("&SSet %s&S.%s&S=\"%s&S\" ok.", section, varname, value);
}

int
setvar(char * section, char * varname, char * value)
{
    ini_state_t stv = {.no_unsafe=1}, *st = &stv;
    st->curr_section = section;

    if (strcasecmp(section, "server") == 0 || strncmp(section, "textcolour", 10) == 0) {

	if (!perm_is_admin()) {
	    printf_chat("&WPermission denied, need to be admin to update system variables");
	    return 0;
	}

	if (!system_ini_fields(st, varname, &value)) {
	    fprintf_logfile("%s: Setfail %s %s = %s", user_id, section, varname, value);
	    printf_chat("&WOption not available &S[%s&S] %s&S = %s", section, varname, value);
	    return 0;
	}

	if (ini_settings->use_port_specific_file) {
	    server_ini_tgt = &server_ini_settings;
	    (void)system_x_ini_fields(st, varname, &value);
	    server_ini_tgt = 0;
	}

	save_system_ini_file(0);

	// Send heartbeat immediatly.
	if (!strcasecmp("private", varname)) {
	    if (alarm_handler_pid)
		kill(alarm_handler_pid, SIGALRM);
	}
    } else {
	if (!perm_level_check(0, 0, 0))
	    return 0;

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
	    save_level_ini(current_level_name);
	} else {
	    fprintf_logfile("%s: Setfailed %s %s = %s", user_id, section, varname, value);
	    printf_chat("&WOption not available &S[%s&S] %s&S = %s&S", section, varname, value);
	    return 0;
	}

    }
    return 1;
}
