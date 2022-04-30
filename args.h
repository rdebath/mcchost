/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#define SERVER_CONF_NAME "server.ini"
typedef struct ini_state_t ini_state_t;
struct ini_state_t {
    FILE * fd;
    int all;	// Write all fields
    int write;	// Set to write fields
    char * curr_section;
};
int system_ini_fields(ini_state_t *st,char *fieldname,char **fieldvalue);
typedef int(*ini_func_t)(ini_state_t *st,char *fieldname,char **value);
int load_ini_file(ini_func_t filetype,char *filename,int quiet);
extern int cpe_disabled;
extern int server_runonce;
extern int server_private;
extern int start_tcp_server;
extern int inetd_mode;
extern int save_conf;
extern char logfile_pattern[1024];
extern int tcp_port_no;
extern int enable_heartbeat_poll;
extern char heartbeat_url[1024];
#define MB_STRLEN 64
#define NB_SLEN (MB_STRLEN+1)
extern char server_secret[NB_SLEN];
extern char server_motd[NB_SLEN];
extern char server_name[NB_SLEN];
void process_args(int argc,char **argv);
