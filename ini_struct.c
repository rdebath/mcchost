#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ini_struct.h"
/*
 * TODO: Should unknown sections give warnings?
 */

#if INTERFACE
typedef int (*ini_func_t)(ini_state_t *st, char * fieldname, char **value);

typedef ini_state_t ini_state_t;
struct ini_state_t {
    FILE * fd;
    int all;	// Write all fields
    int write;	// Set to write fields
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
*/

int
system_ini_fields(ini_state_t *st, char * fieldname, char **fieldvalue)
{
    char *section = "", *fld;
    int found = 0;

    section = "server";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	// -dir --> nope.
	// -log --> call fn, todo.
	// Main level

	INI_STRARRAYCP437("name", server_name);
	INI_STRARRAYCP437("motd", server_motd);
	INI_STRARRAY(st->write?"; salt":"salt", server_secret);	//Base62
	INI_STRARRAY("logfile", logfile_pattern);		//Binary
	INI_BOOLVAL("nocpe", cpe_disabled);
	INI_BOOLVAL("tcp", start_tcp_server);
	INI_BOOLVAL("detach", detach_tcp_server);
	INI_STRARRAY("heartbeat", heartbeat_url);		//ASCII
	INI_BOOLVAL("pollheartbeat", enable_heartbeat_poll);
	INI_INTVAL(st->write && tcp_port_no==25565?"; port":"port", tcp_port_no);
	INI_BOOLVAL("inetd", inetd_mode);
	INI_BOOLVAL("opflag", server_id_op_flag);
	INI_BOOLVAL("private", server_private);
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
    else if (st->curr_section && strcmp("system", st->curr_section) == 0)
	return 1;

    section = "level";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
	INI_INTVAL("size_x", level_prop->cells_x);
	INI_INTVAL("size_y", level_prop->cells_y);
	INI_INTVAL("size_z", level_prop->cells_z);
	INI_INTVAL("spawn_x", level_prop->spawn.x);
	INI_INTVAL("spawn_y", level_prop->spawn.y);
	INI_INTVAL("spawn_z", level_prop->spawn.z);
	INI_INTVAL("spawn_h", level_prop->spawn.h);
	INI_INTVAL("spawn_v", level_prop->spawn.v);
	INI_INTVAL("clickdistance", level_prop->click_distance);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
	INI_STRARRAY("motd", level_prop->motd);
#pragma GCC diagnostic pop

	// hacks_flags

    }

    section = "level.env";
    if (st->all || strcmp(section, st->curr_section) == 0)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
	INI_STRARRAY("texture", level_prop->texname);
#pragma GCC diagnostic pop
	INI_INTVAL("weathertype", level_prop->weather);
	INI_INTVAL("skycolour", level_prop->sky_colour);
	INI_INTVAL("cloudcolour", level_prop->cloud_colour);
	INI_INTVAL("fog_colour", level_prop->fog_colour);
	INI_INTVAL("ambientcolour", level_prop->ambient_colour);
	INI_INTVAL("sunlightcolour", level_prop->sunlight_colour);
	INI_INTVAL("sideblock", level_prop->side_block);
	INI_INTVAL("edgeblock", level_prop->edge_block);
	INI_INTVAL("sidelevel", level_prop->side_level);
	INI_INTVAL("cloudheight", level_prop->clouds_height);
	INI_INTVAL("maxfog", level_prop->max_fog);
	INI_INTVAL("weatherspeed", level_prop->weather_speed);
	INI_INTVAL("weatherfade", level_prop->weather_fade);
	INI_INTVAL("expfog", level_prop->exp_fog);
	INI_INTVAL("skyboxhorspeed", level_prop->skybox_hor_speed);
	INI_INTVAL("skyboxverspeed", level_prop->skybox_ver_speed);
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
	    if (st->write)
		fprintf(st->fd, "\n");
	} else if (strncmp(st->curr_section, "level.blockdef.", 15) == 0) {
	    bn = atoi(st->curr_section+15);
	    if (!bn && st->curr_section[15] == '0') break;
	    if (bn < 0 || bn >= BLOCKMAX) break;
	    sprintf(sectionbuf, "level.blockdef.%d", bn);
	    section = sectionbuf;
	    level_prop->blockdef[bn].defined = 1;
	} else
	    break;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
	INI_STRARRAY("name", level_prop->blockdef[bn].name);
#pragma GCC diagnostic pop
	INI_INTVAL("collide", level_prop->blockdef[bn].collide);
	INI_INTVAL("transparent", level_prop->blockdef[bn].transparent);
	INI_INTVAL("walksound", level_prop->blockdef[bn].walksound);
	INI_INTVAL("blockslight", level_prop->blockdef[bn].blockslight);
	INI_INTVAL("shape", level_prop->blockdef[bn].shape);
	INI_INTVAL("draw", level_prop->blockdef[bn].draw);
	INI_INTVAL("speed", level_prop->blockdef[bn].speed);
	INI_INTVAL("fallback", level_prop->blockdef[bn].fallback);
	INI_INTVAL("texture.0", level_prop->blockdef[bn].textures[0]);
	INI_INTVAL("texture.1", level_prop->blockdef[bn].textures[1]);
	INI_INTVAL("texture.2", level_prop->blockdef[bn].textures[2]);
	INI_INTVAL("texture.3", level_prop->blockdef[bn].textures[3]);
	INI_INTVAL("texture.4", level_prop->blockdef[bn].textures[4]);
	INI_INTVAL("texture.5", level_prop->blockdef[bn].textures[5]);
	INI_INTVAL("fog.0", level_prop->blockdef[bn].fog[0]);
	INI_INTVAL("fog.1", level_prop->blockdef[bn].fog[1]);
	INI_INTVAL("fog.2", level_prop->blockdef[bn].fog[2]);
	INI_INTVAL("fog.3", level_prop->blockdef[bn].fog[3]);
	INI_INTVAL("min.x", level_prop->blockdef[bn].coords[0]);
	INI_INTVAL("min.y", level_prop->blockdef[bn].coords[1]);
	INI_INTVAL("min.z", level_prop->blockdef[bn].coords[2]);
	INI_INTVAL("max.x", level_prop->blockdef[bn].coords[3]);
	INI_INTVAL("max.y", level_prop->blockdef[bn].coords[4]);
	INI_INTVAL("max.z", level_prop->blockdef[bn].coords[5]);

	bn++;
    } while(!st->all || bn < BLOCKMAX);

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
load_ini_file(ini_func_t filetype, char * filename, int quiet)
{
    int errcount = 0;
    ini_state_t st = {0};
    st.fd = fopen(filename, "r");
    if (!st.fd) {
	if (!quiet)
	    printf_chat("&WFile not found: &e%s", filename);
	return -1;
    }

    char ibuf[BUFSIZ];
    while(fgets(ibuf, sizeof(ibuf), st.fd)) {
	char * p = ibuf + strlen(ibuf);
	for(;p>ibuf && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t');p--) p[-1] = 0;

	for(p=ibuf; *p == ' ' || *p == '\t'; p++);
	if (*p == '#' || *p == ';' || *p == 0) continue;
	if (*p == '[') {
	    ini_extract_section(&st, p);
	    continue;
	}
	char label[64];
	int rv = ini_decode_lable(&p, label, sizeof(label));
	if (!rv) {
	    if (quiet)
		fprintf(stderr, "Invalid label %s in %s section %s\n", ibuf, filename, st.curr_section?:"-");
	    else
		printf_chat("&WInvalid label &S%s%W in &S%s&W section &S%s&W\n", ibuf, filename, st.curr_section?:"-");
	    if (++errcount>9) break;
	    continue;
	}
	if (!st.curr_section || !filetype(&st, label, &p)) {
	    if (quiet)
		fprintf(stderr, "Unknown label %s in %s section %s\n", ibuf, filename, st.curr_section?:"-");
	    else
		printf_chat("&WUnknown label &S%s%W in &S%s&W section &S%s&W\n", ibuf, filename, st.curr_section?:"-");
	    if (++errcount>9) break;
	}
    }

    fclose(st.fd);
    if (st.curr_section) free(st.curr_section);
    return -1;
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
	fprintf(st->fd, "[%s]\n", section);
    }
    fprintf(st->fd, "%s =%s%s\n", fieldname, *value?" ":"", value);
}

LOCAL void
ini_write_cp437(ini_state_t *st, char * section, char *fieldname, char *value)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "[%s]\n", section);
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
ini_write_int(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "[%s]\n", section);
    }
    fprintf(st->fd, "%s = %d\n", fieldname, value);
}

LOCAL void
ini_write_bool(ini_state_t *st, char * section, char *fieldname, int value)
{
    if (!st->curr_section || strcmp(st->curr_section, section) != 0) {
	if (st->curr_section) free(st->curr_section);
	st->curr_section = strdup(section);
	fprintf(st->fd, "[%s]\n", section);
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

#define INI_INTVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = atoi(*fieldvalue); \
            else \
                ini_write_int(st, section, fld, (_var)); \
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
