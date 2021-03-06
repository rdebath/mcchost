#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "setvar.h"

#if INTERFACE
#define CMD_SETVAR {N"set", &cmd_setvar}, {N"setvar", &cmd_setvar, .dup=1}, \
    {N"restart", &cmd_restart}
#endif

/*HELP setvar,set H_CMD
&T/set section name value
Sections are &Tserver&S, &Tlevel&S and &Tblock.&WN&S were
&WN&S is the block definition number

The "level" section is used if you don't specify one of
the known sections.

Any value defined as "seconds" may also be set in minutes,
hours or days by adding "m", "h" or "d"

See &T/help set level&S, &T/help set block&S and &T/help set server&S
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
&TPrivate&S Set &TTrue&S to hide registration
&TNoCPE&S Disable negoation of CPE
&TSaveInterval&S Time between level save checks
&TBackupInterval&S Time between level backups

Other options in server.ini can also be set, but may not take effect immediatly.
*/

void
cmd_setvar(char * cmd, char * arg)
{
    char * section = strtok(arg, " ");
    char * varname = 0;
    char * value = 0;
    char vbuf[256];

    if (section) {
	if (strcasecmp(section, "server") == 0 || strcasecmp(section, "level") == 0) {
	    varname = strtok(0, " ");
	    value = strtok(0, "");
	} else if (strcasecmp(section, "block") == 0) {
	    char * bno = strtok(0, " ");
	    snprintf(vbuf, sizeof(vbuf), "block.%s", bno);
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

    if (section == 0 || varname == 0)
	return cmd_help(0, cmd);

    if (!client_trusted) {
	char buf[128];
	sprintf(buf, "%s+", user_id);
	if (strcmp(current_level_name, buf) != 0)
	    return printf_chat("&WPermission denied, only available on level %s", buf);
    }

    ini_state_t stv = {.no_unsafe=1}, *st = &stv;
    st->curr_section = section;

    if (strcasecmp(section, "server") == 0 || strcasecmp(section, "system") == 0) {
	if (!client_trusted)
	    return printf_chat("&WPermission denied, need to be admin.");

	if (!system_ini_fields(st, varname, &value)) {

	    fprintf_logfile("%s: Setfail %s %s = %s", user_id, section, varname, value);
	    printf_chat("&WOption not available &S[%s] %s = %s", section, varname, value);
	    return;
	}

	{
	    char buf[256];
	    snprintf(buf, sizeof(buf), SERVER_CONF_TMP, getpid());
	    if (save_ini_file(system_ini_fields, buf) >= 0) {
		if (rename(buf, SERVER_CONF_NAME) < 0)
		    perror("rename server.ini");
	    }
	    unlink(buf);
	}

	if (!strcasecmp("private", varname)) {
	    if (alarm_handler_pid)
		kill(alarm_handler_pid, SIGALRM);
	}
    } else {
	if (!level_ini_fields(st, varname, &value)) {
	    fprintf_logfile("%s: Setfailed %s %s = %s", user_id, section, varname, value);
	    printf_chat("&WOption not available &S[%s] %s = %s", section, varname, value);
	    return;
	}

	level_prop->dirty_save = 1;
	level_prop->metadata_generation++;
	level_prop->last_modified = time(0);
    }

    printf_chat("&SSet %s.%s=\"%s\" ok.", section, varname, value);
}

/*HELP restart H_CMD
&T/restart
Restarts the server listener process.
*/

void
cmd_restart(char * UNUSED(cmd), char * UNUSED(arg))
{
    if (!client_trusted)
	return printf_chat("&WPermission denied, need to be admin.");

    if (alarm_handler_pid) {
	if (kill(alarm_handler_pid, SIGHUP) < 0) {
	    perror("kill(alarm_handler,SIGHUP)");
	    printf_chat("&WCannot signal listener process");
	} else
	    printf_chat("&SListener process restart triggered");
    } else if (inetd_mode)
	printf_chat("&SNo listener process exists for this session");
    else
	printf_chat("&WListener process not found");
}
