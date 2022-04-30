#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ini_struct.h"

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

/*HELPTEXT inifile
Empty lines are ignored.
Section syntax is normal [...]
    Section syntax uses "." as element seperator within name.
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

	INI_STRARRAY("name", server_name);
	INI_STRARRAY("motd", server_motd);
	INI_STRARRAY("salt", server_secret);
	INI_BOOLVAL("nocpe", cpe_disabled);
	INI_BOOLVAL("tcp", start_tcp_server);
	INI_STRARRAY("heartbeat", heartbeat_url);
	INI_BOOLVAL("pollheartbeat", enable_heartbeat_poll);
	INI_INTVAL("port", tcp_port_no);
	INI_BOOLVAL("inetd", inetd_mode);
	INI_BOOLVAL("opflag", server_id_op_flag);
	INI_BOOLVAL("private", server_private);
	INI_BOOLVAL("runonce", server_runonce);
    }

    // ...

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
	    post_chat(1, "&WFile not found", 0);
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
	    char err[1024];
	    snprintf(err, sizeof(err), "&WInvalid label '%.100s'", ibuf);
	    if (quiet)
		fprintf(stderr, "%s in %s section %s\n", err+2, filename, st.curr_section?:"-");
	    else
		post_chat(1, err, 0);
	    if (++errcount>9) break;
	    continue;
	}
	if (!st.curr_section || !filetype(&st, label, &p)) {
	    char err[1024];
	    snprintf(err, sizeof(err), "&WUnknown label '%.100s'", ibuf);
	    if (quiet)
		fprintf(stderr, "%s in %s section %s\n", err+2, filename, st.curr_section?:"-");
	    else
		post_chat(1, err, 0);
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
	if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z')) {
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
    fprintf(st->fd, "%s = %s\n", fieldname, value);
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
