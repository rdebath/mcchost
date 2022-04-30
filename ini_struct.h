/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#define LOCAL static
typedef struct ini_state_t ini_state_t;
struct ini_state_t {
    FILE * fd;
    int all;	// Write all fields
    int write;	// Set to write fields
    char * curr_section;
};
LOCAL void ini_write_str(ini_state_t *st,char *section,char *fieldname,char *value);
#define INI_STR_PTR(_field, _var, _len) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                snprintf((_var), (_len), "%s", *fieldvalue); \
            else \
                ini_write_str(st, section, fld, (_var)); \
        }
LOCAL void ini_read_bool(int *var,char *value);
LOCAL void ini_write_bool(ini_state_t *st,char *section,char *fieldname,int value);
LOCAL void ini_write_int(ini_state_t *st,char *section,char *fieldname,int value);
LOCAL int ini_decode_lable(char **line,char *buf,int len);
LOCAL int ini_extract_section(ini_state_t *st,char *line);
void post_chat(int where,char *chat,int chat_len);
typedef int(*ini_func_t)(ini_state_t *st,char *fieldname,char **value);
int load_ini_file(ini_func_t filetype,char *filename,int quiet);
void save_ini_file(ini_func_t filetype,char *filename);
extern int server_runonce;
extern int server_private;
extern int server_id_op_flag;
extern int inetd_mode;
extern int tcp_port_no;
#define INI_INTVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                _var = atoi(*fieldvalue); \
            else \
                ini_write_int(st, section, fld, (_var)); \
        }
extern int enable_heartbeat_poll;
extern char heartbeat_url[1024];
extern int start_tcp_server;
extern int cpe_disabled;
#define INI_BOOLVAL(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                ini_read_bool(&(_var), *fieldvalue); \
            else \
                ini_write_bool(st, section, fld, (_var)); \
        }
#define MB_STRLEN 64
#define NB_SLEN (MB_STRLEN+1)
extern char server_secret[NB_SLEN];
extern char server_motd[NB_SLEN];
extern char server_name[NB_SLEN];
#define INI_STRARRAY(_field, _var) \
        fld = _field; \
        if (st->all || strcmp(fieldname, fld) == 0) { \
	    found = 1; \
            if (!st->write) \
                snprintf((_var), sizeof(_var), "%s", *fieldvalue); \
            else \
                ini_write_str(st, section, fld, (_var)); \
        }
int system_ini_fields(ini_state_t *st,char *fieldname,char **fieldvalue);
#define INTERFACE 0
