#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <ctype.h>

#include "ini_struct.h"
/*
 * TODO: Should unknown sections give warnings?
 * TODO: Comment preserving ini file save.
 *
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

#define WC(_x) WC2(!st->no_unsafe, _x)
#define WC2(_c, _x) (st->write && (_c)) ?"; " _x:_x

int
system_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    section = "server";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	INI_STRARRAYCP437("Software", server->software);
	INI_STRARRAYCP437("Name", server->name);
	INI_STRARRAYCP437("Motd", server->motd);
	INI_STRARRAYCP437("Main", server->main_level);

	if (!st->no_unsafe) {
	    INI_STRARRAY("Salt", server->secret);	//Base62
	} else if (st->write) {
	    INI_STRARRAY(WC("Salt"), "XXXXXXXXXXXXXXXX");
	}
	INI_DURATION("KeyRotation", server->key_rotation);

	INI_BOOLVAL("Private", server->private);
	INI_BOOLVAL("NoCPE", server->cpe_disabled);

	if (st->write) fprintf(st->fd, "\n");

	INI_BOOLVAL("tcp", ini_settings.start_tcp_server);
	INI_INTVAL("Port", ini_settings.tcp_port_no);
	INI_BOOLVAL("Inetd", ini_settings.inetd_mode);
	INI_BOOLVAL("Detach", ini_settings.detach_tcp_server);
	INI_STRARRAY("Heartbeat", ini_settings.heartbeat_url);
	INI_BOOLVAL("PollHeartbeat", ini_settings.enable_heartbeat_poll);
	INI_BOOLVAL("Runonce", ini_settings.server_runonce);

	if (st->write) fprintf(st->fd, "\n");

	INI_BOOLVAL("OPFlag", ini_settings.op_flag);
	INI_INTVAL("MaxPlayers", server->max_players);
	INI_STRARRAY(WC2(!*localnet_cidr, "Localnet"), localnet_cidr);

	INI_STRARRAY(WC2(!*logfile_pattern, "Logfile"), logfile_pattern);

	INI_DURATION("SaveInterval", server->save_interval);
	INI_DURATION("BackupInterval", server->backup_interval);

	if (st->write) fprintf(st->fd, "\n");

	INI_BOOLVAL("FlagLogCommands", server->flag_log_commands);
	INI_BOOLVAL("FlagLogChat", server->flag_log_chat);

	if (st->write) fprintf(st->fd, "\n");

	INI_DURATION("AFKInterval", server->afk_interval);
	INI_DURATION("AFKKickInterval", server->afk_kick_interval);

    }

    int tn = 0;
    char sectionbuf[128];
    do
    {
	if (st->all) {
	    if (!textcolour[tn].defined) { tn++; continue; }
	    sprintf(sectionbuf, "textcolour.%d", tn);
	    section = sectionbuf;
	} else {
	    strncpy(sectionbuf, st->curr_section, 11);
	    sectionbuf[11] = 0;
	    if (strcasecmp(sectionbuf, "textcolour.") == 0) {
		tn = atoi(st->curr_section+11);
		if (!tn && st->curr_section[11] == '0') break;
		if (tn < 0 || tn >= 256) break;
		sprintf(sectionbuf, "textcolour.%d", tn);
		section = sectionbuf;
		textcolour[tn].defined = 1;
	    } else
		break;
	}

	if (!st->write || textcolour[tn].name.c[0]) {
	    INI_NBTSTR("Name", textcolour[tn].name);
	}
	if (!st->write || textcolour[tn].colour >= 0) {
	    INI_INTHEX("Colour", textcolour[tn].colour);
	    INI_INTVAL("Alpha", textcolour[tn].a);
	}
	if (!st->write || textcolour[tn].fallback) {
	    INI_INTVAL("Fallback", textcolour[tn].fallback);
	}

	tn++;
    } while(st->all && tn < 256);

    return found;
}


int
level_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    // When writing include a copy of some system stuff.
    // Skip it quietly on read.
    if (st->write) {
	section = "source";
	INI_STRARRAYCP437("Software", server->software);
	INI_STRARRAYCP437("Name", server->name);
	INI_STRARRAYCP437("Motd", server->motd);
	INI_STRARRAYCP437("Level", current_level_name);

    } else if (st->curr_section && strcmp("source", st->curr_section) == 0)
	return 1;

    section = "level";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	if (!st->no_unsafe) {
	    INI_INTVAL("Size.X", level_prop->cells_x);
	    INI_INTVAL("Size.Y", level_prop->cells_y);
	    INI_INTVAL("Size.Z", level_prop->cells_z);
	}

	INI_STRARRAYCP437("Motd", level_prop->motd);
	INI_FIXEDP("Spawn.X", level_prop->spawn.x, 32);
	INI_FIXEDP("Spawn.Y", level_prop->spawn.y, 32);
	INI_FIXEDP("Spawn.Z", level_prop->spawn.z, 32);
	INI_FIXEDP2("Spawn.H", level_prop->spawn.h, 256, 360);
	INI_FIXEDP2("Spawn.V", level_prop->spawn.v, 256, 360);

	INI_FIXEDP("ClickDistance", level_prop->click_distance, 32);

	INI_INTHEX("HacksFlags", level_prop->hacks_flags);

	INI_NBTSTR("Texture", level_prop->texname);
	INI_INTVAL("WeatherType", level_prop->weather);

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

	INI_BOOLVAL("DisallowChange", level_prop->disallowchange);
	INI_BOOLVAL("ReadOnly", level_prop->readonly);
	INI_BOOLVAL("NoUnload", level_prop->no_unload);
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
	    if (strcasecmp(sectionbuf, "block.") == 0) {
		bn = atoi(st->curr_section+6);
		if (!bn && st->curr_section[6] == '0') break;
		if (bn < 0 || bn >= BLOCKMAX) break;
		sprintf(sectionbuf, "block.%d", bn);
		section = sectionbuf;
		level_prop->blockdef[bn].defined = 1;
	    } else
		break;
	}

	INI_BOOLVAL(WC("Defined"), level_prop->blockdef[bn].defined);
	INI_NBTSTR("Name", level_prop->blockdef[bn].name);
	INI_INTVAL("Collide", level_prop->blockdef[bn].collide);
	INI_BOOLVAL("TransmitsLight", level_prop->blockdef[bn].transmits_light);
	INI_INTVAL("WalkSound", level_prop->blockdef[bn].walksound);
	INI_BOOLVAL("FullBright", level_prop->blockdef[bn].fullbright);
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

int
save_ini_file(ini_func_t filetype, char * filename)
{
    ini_state_t st = (ini_state_t){.all=1, .write=1};
    st.fd = fopen(filename, "w");
    if (!st.fd) {
	perror(filename);
	return -1;
    }
    filetype(&st,0,0);
    fclose(st.fd);
    if (st.curr_section) free(st.curr_section);
    return 0;
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
	p = ini_extract_section(st, p);
	if (p) for(; *p == ' ' || *p == '\t'; p++);
	if (!p || *p == 0)
	    return 1;
    }
    char label[64];
    int rv = ini_decode_lable(&p, label, sizeof(label));
    if (!rv) {
	if (st->quiet)
	    printlog("Invalid label %s in %s section %s", ibuf, st->filename, st->curr_section?:"-");
	else
	    printf_chat("&WInvalid label &S%s&W in &S%s&W section &S%s&W", ibuf, st->filename, st->curr_section?:"-");
	return 0;
    }
    if (!st->curr_section || !filetype(st, label, &p)) {
	if (st->quiet) {
	    printlog("Unknown item \"%s\" in file \"%s\" section \"%s\" -- label \"%s\" value \"%s\"",
		ibuf, st->filename, st->curr_section?:"-", label, p);
	} else
	    printf_chat("&WUnknown item&S \"%s\" section \"%s\"", ibuf, st->curr_section?:"-");
	return 0;
    }
    return 1;
}

LOCAL char *
ini_extract_section(ini_state_t *st, char *line)
{
    if (st->curr_section) { free(st->curr_section); st->curr_section = 0; }

    char buf[BUFSIZ];
    char *d = buf, *p = line;
    if (*p != '[') return p;
    for(p++; *p; p++) {
	if (*p == ' ' || *p == '_' || *p == '\t') continue;
	if (*p == ']') { p++; break; }
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
    if (*buf)
	st->curr_section = strdup(buf);
    return p;
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
ini_write_section(ini_state_t *st, char * section)
{
    if (!st->write) return;
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "\n[%s]\n", section);
    }
}

LOCAL void
ini_write_str(ini_state_t *st, char * section, char *fieldname, char *value)
{
    ini_write_section(st, section);
    fprintf(st->fd, "%s =%s%s\n", fieldname, *value?" ":"", value);
}

LOCAL void
ini_read_str(char * buf, int len, char *value)
{
    snprintf(buf, len, "%s", value);
}

LOCAL void
ini_write_cp437(ini_state_t *st, char * section, char *fieldname, char *value)
{
    ini_write_section(st, section);
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
ini_write_nbtstr(ini_state_t *st, char * section, char *fieldname, nbtstr_t *value)
{
    ini_write_section(st, section);
    fprintf(st->fd, "%s =%s", fieldname, value->c[0]?" ":"\n");
    if (value->c[0] == 0) return;
    for(int i = 0; value->c[i]; i++)
	cp437_prt(st->fd, value->c[i]);
    fputc('\n', st->fd);
}

LOCAL void
ini_read_nbtstr(nbtstr_t * buf, char *value)
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
    ini_write_section(st, section);
    fprintf(st->fd, "%s = %d\n", fieldname, value);
}

LOCAL void
ini_write_int_blk(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (value < 0 || value >= BLOCKMAX) return; // Skip illegal values.

    ini_write_section(st, section);
    fprintf(st->fd, "%s = %d\n", fieldname, value);
}

LOCAL void
ini_write_hexint(ini_state_t *st, char * section, char *fieldname, int value)
{
    ini_write_section(st, section);
    if (value < 0)
	fprintf(st->fd, "%s = %d\n", fieldname, value);
    else
	fprintf(st->fd, "%s = 0x%x\n", fieldname, value);
}

LOCAL void
ini_write_bool(ini_state_t *st, char * section, char *fieldname, int value)
{
    ini_write_section(st, section);
    if (value < 0 || value > 1)
	fprintf(st->fd, "%s = %d\n", fieldname, value);
    else
	fprintf(st->fd, "%s = %s\n", fieldname, value?"true":"false");
}

LOCAL int
ini_read_bool(char * value)
{
    int rv = 0;
    if (strcasecmp(value, "true") == 0) rv = 1; else
    if (strcasecmp(value, "yes") == 0) rv = 1; else
    if (strcasecmp(value, "on") == 0) rv = 1; else
    if ((*value >= '0' && *value <= '9') || *value == '-')
	rv = atoi(value);
    return rv;
}

LOCAL int
ini_read_int_scale(char * value, int scalefactor, int scalefactor2)
{
    double nval = strtod(value, 0);
    nval = round(nval * scalefactor / scalefactor2);
    return (int)nval;
}

LOCAL void
ini_write_int_scale(ini_state_t *st, char * section, char *fieldname, int value, int scalefactor, int scalefactor2)
{
    ini_write_section(st, section);
    double svalue = (double)value * scalefactor2 / scalefactor;
    int digits = 0, sd = 1;
    while (sd < scalefactor / scalefactor2) { digits++; sd *= 10; }
    fprintf(st->fd, "%s = %.*f\n", fieldname, digits, svalue);
}

static struct time_units_t { char id; int scale; } time_units[] = 
{
    {'s', 1},
    {'m', 60},
    {'h', 3600},
    {'d', 24*3600},
    {'w', 7*24*3600},
    {0, 0}
};

LOCAL int
ini_read_int_duration(char * value)
{
    char * unit = 0;
    int nval = strtod(value, &unit);
    int ch = tolower((uint8_t)*unit);
    for(int i = 0; time_units[i].id; i++)
	if (ch == time_units[i].id) {
	    nval *= time_units[i].scale;
	    break;
	}
    return nval;
}

LOCAL void
ini_write_int_duration(ini_state_t *st, char * section, char *fieldname, int value)
{
    ini_write_section(st, section);
    int unit = 0;
    if (value)
	for(int i = 0; time_units[i].id; i++)
	    if (value/time_units[i].scale*time_units[i].scale == value)
		unit = i;
    fprintf(st->fd, "%s = %d%c\n", fieldname,
	value/time_units[unit].scale, time_units[unit].id);
}


#if INTERFACE
#define INI_STRARRAY(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_str((_var), sizeof(_var), *fieldvalue); \
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
                _var = ini_read_int_scale(*fieldvalue, _scale, 1); \
            else \
                ini_write_int_scale(st, section, fld, (_var), _scale, 1); \
        }

#define INI_FIXEDP2(_field, _var, _scale, _scale2) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_int_scale(*fieldvalue, _scale, _scale2); \
            else \
                ini_write_int_scale(st, section, fld, (_var), _scale, _scale2); \
        }

#define INI_DURATION(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_int_duration(*fieldvalue); \
            else \
                ini_write_int_duration(st, section, fld, (_var)); \
        }

#define INI_BOOLVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcasecmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_bool(*fieldvalue); \
            else \
                ini_write_bool(st, section, fld, (_var)); \
        }

#endif
