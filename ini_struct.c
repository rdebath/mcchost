#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ini_struct.h"
/*
 * TODO: Should unknown sections give warnings?
 * TODO: Comment preserving ini file save.
 *
 * Server.MaxPlayers
 * Server.WhiteList
 * Server.Antispam
 */

#if INTERFACE
#include <stdio.h>

typedef int (*ini_func_t)(ini_state_t *st, char * fieldname, char **value);

typedef ini_state_t ini_state_t;
struct ini_state_t {
    FILE * fd;
    char * filename;

    int all;	// Process all fields
    int write;	// Set to write fields
    int quiet;	// Don't comment to the remote (use stderr)
    int no_unsafe;
    char * curr_section;
};

#define CMD_SETVAR  {N"setvar", &cmd_setvar}, {N"set", &cmd_setvar}

#endif

/*HELP inifile
Empty lines are ignored.
Section syntax is normal [...]
    Section syntax uses "." as element separator within name.
    Elements may be numeric for array index.
    Other elements are identifiers.

Comment lines have a "#" or ";" at the start of the line
Labels are followed by "=" or ":"
Spaces and tabs before "=", ":", ";", "#" are ignored.

Value is trimmed at both ends for spaces and tabs.

Quotes are not special.
Backslashes are not special
Strings are generally limited to 64 cp437 characters by protocol.
Character set of file is UTF8
*/

int
system_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    section = "server";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	INI_STRARRAYCP437("Name", server.name);
	INI_STRARRAYCP437("Motd", server.motd);
	INI_STRARRAYCP437("Main", server.main_level);
	INI_STRARRAY(st->write?"; Salt":"Salt", server.secret);	//Base62
	INI_BOOLVAL("NoCPE", server.cpe_disabled);
	INI_BOOLVAL("Private", server.private);

	INI_STRARRAY("Logfile", logfile_pattern);		//Binary
	INI_BOOLVAL("tcp", start_tcp_server);
	INI_BOOLVAL("Detach", detach_tcp_server);
	INI_STRARRAY("Heartbeat", heartbeat_url);		//ASCII
	INI_BOOLVAL("PollHeartbeat", enable_heartbeat_poll);
	INI_INTVAL(st->write && tcp_port_no==25565?"; Port":"Port", tcp_port_no);
	INI_BOOLVAL("Inetd", inetd_mode);
	INI_BOOLVAL("OPFlag", server_id_op_flag);
	INI_BOOLVAL(st->write?"; Runonce":"Runonce", server_runonce);
    }

    return found;
}


int
level_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    // When writing include a copy of the system stuff.
    // Skip it quietly on read.
    if (st->write)
	system_ini_fields(st, fieldname, fieldvalue);
    else if (st->curr_section && strcmp("server", st->curr_section) == 0)
	return 1;

    section = "level";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	if (!st->no_unsafe) {
	    INI_INTVAL("Size.X", level_prop->cells_x);
	    INI_INTVAL("Size.Y", level_prop->cells_y);
	    INI_INTVAL("Size.Z", level_prop->cells_z);
	}

	INI_NBTSTR("Motd", level_prop->motd);
	INI_FIXEDP("Spawn.X", level_prop->spawn.x, 32);
	INI_FIXEDP("Spawn.Y", level_prop->spawn.y, 32);
	INI_FIXEDP("Spawn.Z", level_prop->spawn.z, 32);
	INI_INTVAL("Spawn.H", level_prop->spawn.h);
	INI_INTVAL("Spawn.V", level_prop->spawn.v);

	INI_FIXEDP("ClickDistance", level_prop->click_distance, 32);

	INI_INTHEX("HacksFlags", level_prop->hacks_flags);

	INI_NBTSTR("Texture", level_prop->texname);
	INI_INTVAL("EnvWeatherType", level_prop->weather);

	INI_INTHEX("SkyColour", level_prop->sky_colour);
	INI_INTHEX("CloudColour", level_prop->cloud_colour);
	INI_INTHEX("FogColour", level_prop->fog_colour);
	INI_INTHEX("AmbientColour", level_prop->ambient_colour);
	INI_INTHEX("SunlightColour", level_prop->sunlight_colour);
	INI_INTHEX("SkyboxColour", level_prop->skybox_colour);

	INI_INTVAL("SideBlock", level_prop->side_block);
	INI_INTVAL("EdgeBlock", level_prop->edge_block);
	INI_INTVAL("SideLevel", level_prop->side_level);
	INI_INTVAL("SideOffset", level_prop->side_offset);

	INI_INTVAL("CloudHeight", level_prop->clouds_height);
	INI_INTVAL("MaxFog", level_prop->max_fog);

	INI_FIXEDP("CloudsSpeed", level_prop->clouds_speed, 256);
	INI_FIXEDP("WeatherSpeed", level_prop->weather_speed, 256);
	INI_FIXEDP("WeatherFade", level_prop->weather_fade, 128);
	INI_INTVAL("ExpFog", level_prop->exp_fog);
	INI_FIXEDP("SkyboxHorSpeed", level_prop->skybox_hor_speed, 1024);
	INI_FIXEDP("SkyboxVerSpeed", level_prop->skybox_ver_speed, 1024);
    }

    int bn = 0;
    char sectionbuf[128];
    do
    {
	if (st->all) {
	    if (!level_prop->blockdef[bn].defined) { bn++; continue; }
	    sprintf(sectionbuf, "block.%d", bn);
	    section = sectionbuf;
	} else {
	    strncpy(sectionbuf, st->curr_section, 6);
	    sectionbuf[6] = 0;
	    if (strcasecmp(st->curr_section, "block.") == 0) {
		bn = atoi(st->curr_section+6);
		if (!bn && st->curr_section[6] == '0') break;
		if (bn < 0 || bn >= BLOCKMAX) break;
		sprintf(sectionbuf, "block.%d", bn);
		section = sectionbuf;
		level_prop->blockdef[bn].defined = 1;
	    } else
		break;
	}

	INI_NBTSTR("Name", level_prop->blockdef[bn].name);
	INI_INTVAL("Collide", level_prop->blockdef[bn].collide);
	INI_INTVAL("TransmitsLight", level_prop->blockdef[bn].transmits_light);
	INI_INTVAL("WalkSound", level_prop->blockdef[bn].walksound);
	INI_INTVAL("FullBright", level_prop->blockdef[bn].fullbright);
	INI_INTVAL("Shape", level_prop->blockdef[bn].shape);
	INI_INTVAL("Draw", level_prop->blockdef[bn].draw);
	INI_FIXEDP("Speed", level_prop->blockdef[bn].speed, 1000);
	INI_BLKVAL("Fallback", level_prop->blockdef[bn].fallback);
	INI_INTVAL("Texture.Top", level_prop->blockdef[bn].textures[0]);
	INI_INTVAL("Texture.Left", level_prop->blockdef[bn].textures[1]);
	INI_INTVAL("Texture.Right", level_prop->blockdef[bn].textures[2]);
	INI_INTVAL("Texture.Front", level_prop->blockdef[bn].textures[3]);
	INI_INTVAL("Texture.Back", level_prop->blockdef[bn].textures[4]);
	INI_INTVAL("Texture.Bottom", level_prop->blockdef[bn].textures[5]);
	INI_INTVAL("Fog.Den", level_prop->blockdef[bn].fog[0]);
	INI_INTVAL("Fog.R", level_prop->blockdef[bn].fog[1]);
	INI_INTVAL("Fog.G", level_prop->blockdef[bn].fog[2]);
	INI_INTVAL("Fog.B", level_prop->blockdef[bn].fog[3]);
	INI_INTVAL("Min.X", level_prop->blockdef[bn].coords[0]);
	INI_INTVAL("Min.Y", level_prop->blockdef[bn].coords[1]);
	INI_INTVAL("Min.Z", level_prop->blockdef[bn].coords[2]);
	INI_INTVAL("Max.X", level_prop->blockdef[bn].coords[3]);
	INI_INTVAL("Max.Y", level_prop->blockdef[bn].coords[4]);
	INI_INTVAL("Max.Z", level_prop->blockdef[bn].coords[5]);

	INI_BLKVAL("StackBlock", level_prop->blockdef[bn].stack_block);
	INI_BLKVAL("GrassBlock", level_prop->blockdef[bn].grass_block);
	INI_BLKVAL("DirtBlock", level_prop->blockdef[bn].dirt_block);
	INI_BLKVAL("Order", level_prop->blockdef[bn].inventory_order);

	bn++;
    } while(st->all && bn < BLOCKMAX);

    return found;
}

void
save_ini_file(ini_func_t filetype, char * filename)
{
    ini_state_t st = (ini_state_t){.all=1, .write=1};
    st.fd = fopen(filename, "w");
    if (!st.fd) {
	perror(filename);
	return;
    }
    filetype(&st,0,0);
    fclose(st.fd);
    if (st.curr_section) free(st.curr_section);
}

int
load_ini_file(ini_func_t filetype, char * filename, int quiet, int no_unsafe)
{
    int rv = 0;
    ini_state_t st = {.quiet = quiet, .no_unsafe=no_unsafe, .filename = filename};
    FILE *ifd = fopen(filename, "r");
    if (!ifd) {
	if (!quiet)
	    printf_chat("&WFile not found: &e%s", filename);
	return -1;
    }

    char ibuf[BUFSIZ];
    while(fgets(ibuf, sizeof(ibuf), ifd)) {
	if (load_ini_line(&st, filetype, ibuf) == 0) { rv = -1; break; }
    }

    fclose(ifd);
    if (st.curr_section) free(st.curr_section);
    return rv;
}

int
load_ini_line(ini_state_t *st, ini_func_t filetype, char *ibuf)
{
    char * p = ibuf + strlen(ibuf);
    for(;p>ibuf && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t');p--) p[-1] = 0;

    for(p=ibuf; *p == ' ' || *p == '\t'; p++);
    if (*p == '#' || *p == ';' || *p == 0) return 1;
    if (*p == '[') {
	ini_extract_section(st, p);
	return 1;
    }
    char label[64];
    int rv = ini_decode_lable(&p, label, sizeof(label));
    if (!rv) {
	if (st->quiet)
	    fprintf(stderr, "Invalid label %s in %s section %s\n", ibuf, st->filename, st->curr_section?:"-");
	else
	    printf_chat("&WInvalid label &S%s&W in &S%s&W section &S%s&W", ibuf, st->filename, st->curr_section?:"-");
	return 0;
    }
    if (!st->curr_section || !filetype(st, label, &p)) {
	if (st->quiet)
	    fprintf(stderr, "Unknown label %s in %s section %s\n", ibuf, st->filename, st->curr_section?:"-");
	else
	    printf_chat("&WUnknown label &S%s&W in &S%s&W section &S%s&W", ibuf, st->filename, st->curr_section?:"-");
	return 0;
    }
    return 1;
}

LOCAL int
ini_extract_section(ini_state_t *st, char *line)
{
    if (st->curr_section) { free(st->curr_section); st->curr_section = 0; }

    char buf[BUFSIZ];
    char *d = buf, *p = line;
    if (*p != '[') return 0;
    for(p++; *p; p++) {
	if (*p == ' ' || *p == '_' || *p == '\t') continue;
	if (*p == ']') break;
	if (*p >= 'A' && *p <= 'Z') {
	    *d++ = *p - 'A' + 'a';
	    continue;
	}
	if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || *p == '.') {
	    *d++ = *p;
	    continue;
	}
	if (*p == '/' || *p == '\\' || *p == '-')
	    *d++ = '.';
	// Other chars are ignored.
    }
    *d = 0;
    if (*buf == 0) return 0;
    st->curr_section = strdup(buf);
    return 1;
}

LOCAL int
ini_decode_lable(char **line, char *buf, int len)
{
    char * d = buf, *p = *line;
    for( ; *p; p++) {
	if (*p == ' ' || *p == '_' || *p == '\t') continue;
	if (*p == '=' || *p == ':') break;
	if (*p >= 'A' && *p <= 'Z') {
	    if (d<buf+len-1)
		*d++ = *p - 'A' + 'a';
	    continue;
	}
	if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || *p == '.') {
	    if (d<buf+len-1)
		*d++ = *p;
	    continue;
	}
	// Other chars are ignored.
    }
    *d = 0;
    if (*buf == 0) return 0;
    if (*p != ':' && *p != '=') return 0;
    p++;

    while(*p == ' ' || *p == '\t') p++;
    *line = p;
    return 1;
}

LOCAL void
ini_write_str(ini_state_t *st, char * section, char *fieldname, char *value)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
    fprintf(st->fd, "%s =%s%s\n", fieldname, *value?" ":"", value);
}

LOCAL void
ini_write_cp437(ini_state_t *st, char * section, char *fieldname, char *value)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
    fprintf(st->fd, "%s =%s", fieldname, *value?" ":"\n");
    if (*value == 0) return;
    while(*value)
	cp437_prt(st->fd, *value++);
    fputc('\n', st->fd);
}

LOCAL void
ini_read_cp437(char * buf, int len, char *value)
{
    int vlen = strlen(value);
    convert_to_cp437(value, &vlen);
    if (vlen >= len) vlen = len-1;
    memcpy(buf, value, vlen);
    buf[vlen] = 0;
}

LOCAL void
ini_write_nbtstr(ini_state_t *st, char * section, char *fieldname, volatile nbtstr_t *value)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
    fprintf(st->fd, "%s =%s", fieldname, value->c[0]?" ":"\n");
    if (value->c[0] == 0) return;
    for(int i = 0; value->c[i]; i++)
	cp437_prt(st->fd, value->c[i]);
    fputc('\n', st->fd);
}

LOCAL void
ini_read_nbtstr(volatile nbtstr_t * buf, char *value)
{
    int vlen = strlen(value);
    nbtstr_t t = {0};
    convert_to_cp437(value, &vlen);
    if (vlen >= NB_SLEN) vlen = NB_SLEN-1;
    memcpy(t.c, value, vlen);
    t.c[vlen] = 0;
    *buf = t;
}

LOCAL void
ini_write_int(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
    fprintf(st->fd, "%s = %d\n", fieldname, value);
}

LOCAL void
ini_write_int_blk(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (value < 0 || value >= BLOCKMAX) return; // Skip illegal values.

    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
    fprintf(st->fd, "%s = %d\n", fieldname, value);
}

LOCAL void
ini_write_hexint(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
    if (value < 0)
	fprintf(st->fd, "%s = %d\n", fieldname, value);
    else
	fprintf(st->fd, "%s = 0x%x\n", fieldname, value);
}

LOCAL void
ini_write_bool(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
    fprintf(st->fd, "%s = %s\n", fieldname, value?"true":"false");
}

LOCAL void
ini_read_bool(int *var, char * value)
{
    if (strcasecmp(value, "true") == 0) *var = 1; else
    if (strcasecmp(value, "yes") == 0) *var = 1; else
    if (atoi(value) != 0) *var = 1; else
    *var = 0;
}

LOCAL int
ini_read_int_scale(char * value, int scalefactor)
{
    double nval = strtod(value, 0);
    nval = round(nval * scalefactor);
    return (int)nval;
}

LOCAL void
ini_write_int_scale(ini_state_t *st, char * section, char *fieldname, int value, int scalefactor)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
    double svalue = (double)value / scalefactor;
    int digits = 0, sd = 1;
    while (sd < scalefactor) { digits++; sd *= 10; }
    fprintf(st->fd, "%s = %.*f\n", fieldname, digits, svalue);
}

#if INTERFACE
#define INI_STRARRAY(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                snprintf((_var), sizeof(_var), "%s", *fieldvalue); \
            else \
                ini_write_str(st, section, fld, (_var)); \
        }

#define INI_STRARRAYCP437(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_cp437((_var), sizeof(_var), *fieldvalue); \
            else \
                ini_write_cp437(st, section, fld, (_var)); \
        }

#define INI_NBTSTR(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_nbtstr(&(_var), *fieldvalue); \
            else \
                ini_write_nbtstr(st, section, fld, &(_var)); \
        }

#define INI_INTVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = atoi(*fieldvalue); \
            else \
                ini_write_int(st, section, fld, (_var)); \
        }

#define INI_BLKVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = atoi(*fieldvalue); \
            else \
                ini_write_int_blk(st, section, fld, (_var)); \
        }

#define INI_INTHEX(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = strtol(*fieldvalue, 0, 0); \
            else \
                ini_write_hexint(st, section, fld, (_var)); \
        }

#define INI_FIXEDP(_field, _var, _scale) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_int_scale(*fieldvalue, _scale); \
            else \
                ini_write_int_scale(st, section, fld, (_var), _scale); \
        }

#define INI_BOOLVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_bool(&(_var), *fieldvalue); \
            else \
                ini_write_bool(st, section, fld, (_var)); \
        }

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

void
cmd_setvar(UNUSED char * cmd, char * arg)
{
    char * section = strtok(arg, " ");
    char * varname = strtok(0, " ");
    char * value = strtok(0, "");

    if (section == 0 || varname == 0)
	return cmd_help(0, cmd);
    if (!client_ipv4_localhost) {
	char buf[128];
	sprintf(buf, "%s+", user_id);
	if (strcmp(current_level_name, buf) != 0)
	    return printf_chat("&WPermission denied, only available on level %s", buf);
    }

    if (value == 0) value = "";

    fprintf(stderr, "%s: [%s]%s= %s\n", user_id, section, varname, value);

    ini_state_t stv = {.no_unsafe=1}, *st = &stv;
    st->curr_section = section;

    if (strcasecmp(section, "server") == 0) {
	if (!system_ini_fields(st, varname, &value)) {
	    if (!client_ipv4_localhost)
		return printf_chat("&WPermission denied, need to be localhost.");
	    printf_chat("&WOption not available &S[%s] %s= %s", section, varname, value);
	    return;
	}
    } else
    if (!level_ini_fields(st, varname, &value)) {
	printf_chat("&WOption not available &S[%s] %s= %s", section, varname, value);
	return;
    }

    level_prop->dirty_save = 1;
    level_prop->metadata_generation++;
}
