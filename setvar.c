#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "setvar.h"

#if INTERFACE
#define CMD_SETVAR  {N"setvar", &cmd_setvar, .dup=1}, {N"set", &cmd_setvar}
#endif

/*HELP setvar,set H_CMD
&T/set section name value
Sections are &Tlevel&S and &Tblock.&WN&S were &WN&S is the block definition number

Options in "level" include ...
Spawn.X Spawn.Y Spawn.Z Spawn.H Spawn.V
Motd ClickDistance Texture EnvWeatherType SkyColour
CloudColour FogColour AmbientColour SunlightColour
SkyboxColour SideBlock EdgeBlock SideLevel SideOffset
CloudHeight MaxFog CloudsSpeed WeatherSpeed WeatherFade
ExpFog SkyboxHorSpeed SkyboxVerSpeed

*/

/*HELP set_block H_CMD
&W TODO
*/

/*HELP set_system H_CMD
&W TODO
*/

void
cmd_setvar(UNUSED char * cmd, char * arg)
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
	if (!system_ini_fields(st, varname, &value)) {
	    if (!client_trusted)
		return printf_chat("&WPermission denied, need to be admin.");

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
	    fprintf_logfile("%s: Setfail %s %s = %s", user_id, section, varname, value);
	    printf_chat("&WOption not available &S[%s] %s = %s", section, varname, value);
	    return;
	}

	level_prop->dirty_save = 1;
	level_prop->metadata_generation++;
	level_prop->last_modified = time(0);
    }

    fprintf_logfile("%s: Set %s %s = %s", user_id, section, varname, value);
    printf_chat("&SSet %s.%s=\"%s\" ok.", section, varname, value);
}
