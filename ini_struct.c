#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ini_struct.h"
/*
 * TODO: Should unknown sections give warnings?
 * TODO: Comment preserving ini file save.
 */

#if INTERFACE
typedef int (*ini_func_t)(ini_state_t *st, char * fieldname, char **value);

typedef ini_state_t ini_state_t;
struct ini_state_t {
    FILE * fd;
    char * filename;

    int all;	// Write all fields
    int write;	// Set to write fields
    int quiet;	// Don't comment to the remote (use stderr)
    int errcount;
    int no_unsafe;
    char * curr_section;
};

#define CMD_SETVAR  {N"setvar", &cmd_setvar}

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
	INI_STRARRAYCP437("name", server.name);
	INI_STRARRAYCP437("motd", server.motd);
	INI_STRARRAYCP437("main", server.main_level);
	INI_STRARRAY(st->write?"; salt":"salt", server.secret);	//Base62
	INI_BOOLVAL("nocpe", server.cpe_disabled);
	INI_BOOLVAL("private", server.private);

	INI_STRARRAY("logfile", logfile_pattern);		//Binary
	INI_BOOLVAL("tcp", start_tcp_server);
	INI_BOOLVAL("detach", detach_tcp_server);
	INI_STRARRAY("heartbeat", heartbeat_url);		//ASCII
	INI_BOOLVAL("pollheartbeat", enable_heartbeat_poll);
	INI_INTVAL(st->write && tcp_port_no==25565?"; port":"port", tcp_port_no);
	INI_BOOLVAL("inetd", inetd_mode);
	INI_BOOLVAL("opflag", server_id_op_flag);
	INI_BOOLVAL(st->write?"; runonce":"runonce", server_runonce);
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
	    INI_INTVAL("size.x", level_prop->cells_x);
	    INI_INTVAL("size.y", level_prop->cells_y);
	    INI_INTVAL("size.z", level_prop->cells_z);
	}

	INI_NBTSTR("motd", level_prop->motd);
	INI_INTSCALE("spawn.x", level_prop->spawn.x, 32);
	INI_INTSCALE("spawn.y", level_prop->spawn.y, 32);
	INI_INTSCALE("spawn.z", level_prop->spawn.z, 32);
	INI_INTVAL("spawn.h", level_prop->spawn.h);
	INI_INTVAL("spawn.v", level_prop->spawn.v);

	INI_INTSCALE("clickdistance", level_prop->click_distance, 32);
	// hacks_flags

	INI_NBTSTR("texture", level_prop->texname);
	INI_INTVAL("weathertype", level_prop->weather);

	INI_INTHEX("skycolour", level_prop->sky_colour);
	INI_INTHEX("cloudcolour", level_prop->cloud_colour);
	INI_INTHEX("fogcolour", level_prop->fog_colour);
	INI_INTHEX("ambientcolour", level_prop->ambient_colour);
	INI_INTHEX("sunlightcolour", level_prop->sunlight_colour);
	INI_INTHEX("skyboxcolour", level_prop->skybox_colour);

	INI_INTVAL("sideblock", level_prop->side_block);
	INI_INTVAL("edgeblock", level_prop->edge_block);
	INI_INTVAL("sidelevel", level_prop->side_level);
	INI_INTVAL("sideoffset", level_prop->side_offset);

	INI_INTVAL("cloudheight", level_prop->clouds_height);
	INI_INTVAL("maxfog", level_prop->max_fog);

	INI_INTSCALE("cloudsspeed", level_prop->clouds_speed, 256);
	INI_INTSCALE("weatherspeed", level_prop->weather_speed, 256);
	INI_INTSCALE("weatherfade", level_prop->weather_fade, 128);
	INI_INTVAL("expfog", level_prop->exp_fog);
	INI_INTSCALE("skyboxhorspeed", level_prop->skybox_hor_speed, 1024);
	INI_INTSCALE("skyboxverspeed", level_prop->skybox_ver_speed, 1024);
    }

    int bn = 0;
    char sectionbuf[128];
    do
    {
	//struct blockdef_t blockdef[BLOCKMAX];
	//int invt_order[BLOCKMAX];
	//uint8_t block_perms[BLOCKMAX];

	if (st->all) {
	    if (!level_prop->blockdef[bn].defined) { bn++; continue; }
	    sprintf(sectionbuf, "level.blockdef.%d", bn);
	    section = sectionbuf;
	} else if (strncmp(st->curr_section, "level.blockdef.", 15) == 0) {
	    bn = atoi(st->curr_section+15);
	    if (!bn && st->curr_section[15] == '0') break;
	    if (bn < 0 || bn >= BLOCKMAX) break;
	    sprintf(sectionbuf, "level.blockdef.%d", bn);
	    section = sectionbuf;
	    level_prop->blockdef[bn].defined = 1;
	} else
	    break;

	INI_NBTSTR("name", level_prop->blockdef[bn].name);
	INI_INTVAL("collide", level_prop->blockdef[bn].collide);
	INI_INTVAL("transmitslight", level_prop->blockdef[bn].transmits_light);
	INI_INTVAL("walksound", level_prop->blockdef[bn].walksound);
	INI_INTVAL("fullbright", level_prop->blockdef[bn].fullbright);
	INI_INTVAL("shape", level_prop->blockdef[bn].shape);
	INI_INTVAL("draw", level_prop->blockdef[bn].draw);
	INI_INTSCALE("speed", level_prop->blockdef[bn].speed, 1000);
	INI_BLKVAL("fallback", level_prop->blockdef[bn].fallback);
	INI_INTVAL("texture.top", level_prop->blockdef[bn].textures[0]);
	INI_INTVAL("texture.left", level_prop->blockdef[bn].textures[1]);
	INI_INTVAL("texture.right", level_prop->blockdef[bn].textures[2]);
	INI_INTVAL("texture.front", level_prop->blockdef[bn].textures[3]);
	INI_INTVAL("texture.back", level_prop->blockdef[bn].textures[4]);
	INI_INTVAL("texture.bottom", level_prop->blockdef[bn].textures[5]);
	INI_INTVAL("fog.den", level_prop->blockdef[bn].fog[0]);
	INI_INTVAL("fog.r", level_prop->blockdef[bn].fog[1]);
	INI_INTVAL("fog.g", level_prop->blockdef[bn].fog[2]);
	INI_INTVAL("fog.b", level_prop->blockdef[bn].fog[3]);
	INI_INTVAL("min.x", level_prop->blockdef[bn].coords[0]);
	INI_INTVAL("min.y", level_prop->blockdef[bn].coords[1]);
	INI_INTVAL("min.z", level_prop->blockdef[bn].coords[2]);
	INI_INTVAL("max.x", level_prop->blockdef[bn].coords[3]);
	INI_INTVAL("max.y", level_prop->blockdef[bn].coords[4]);
	INI_INTVAL("max.z", level_prop->blockdef[bn].coords[5]);

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
	// if (++st->errcount>9) return -1;
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
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                snprintf((_var), sizeof(_var), "%s", *fieldvalue); \
            else \
                ini_write_str(st, section, fld, (_var)); \
        }

#define INI_STR_PTR(_field, _var, _len) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                snprintf((_var), (_len), "%s", *fieldvalue); \
            else \
                ini_write_str(st, section, fld, (_var)); \
        }

#define INI_STRARRAYCP437(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_cp437((_var), sizeof(_var), *fieldvalue); \
            else \
                ini_write_cp437(st, section, fld, (_var)); \
        }

#define INI_NBTSTR(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_nbtstr(&(_var), *fieldvalue); \
            else \
                ini_write_nbtstr(st, section, fld, &(_var)); \
        }

#define INI_INTVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = atoi(*fieldvalue); \
            else \
                ini_write_int(st, section, fld, (_var)); \
        }

#define INI_BLKVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = atoi(*fieldvalue); \
            else \
                ini_write_int_blk(st, section, fld, (_var)); \
        }

#define INI_INTHEX(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = strtol(*fieldvalue, 0, 0); \
            else \
                ini_write_hexint(st, section, fld, (_var)); \
        }

#define INI_INTSCALE(_field, _var, _scale) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = ini_read_int_scale(*fieldvalue, _scale); \
            else \
                ini_write_int_scale(st, section, fld, (_var), _scale); \
        }

#define INI_BOOLVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_bool(&(_var), *fieldvalue); \
            else \
                ini_write_bool(st, section, fld, (_var)); \
        }

#endif


/*HELP setvar
&T/setvar section name value
Sections are &Tlevel&S and &Tlevel.blockdef.&WN&S were &WN&S is the block definition number

*/
void
cmd_setvar(UNUSED char * cmd, char * arg)
{
    char * section = strtok(arg, " ");
    char * varname = strtok(0, " ");
    char * value = strtok(0, "");

    if (section == 0 || varname == 0 || !client_ipv4_localhost) {
	printf_chat("&WUsage");
	return;
    }
    if (value == 0) value = "";

    fprintf(stderr, "%s: [%s]%s= %s\n", user_id, section, varname, value);

    ini_state_t stv = {.no_unsafe=1}, *st = &stv;
    st->curr_section = section;

    if (strcasecmp(section, "server") == 0) {
	if (!system_ini_fields(st, varname, &value)) {
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
