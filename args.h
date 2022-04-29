/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
extern int ignore_cpe;
extern int server_runonce;
extern int server_private;
extern int start_tcp_server;
void set_logfile(char *logfile_name);
extern int tcp_port_no;
extern int enable_heartbeat_poll;
extern char heartbeat_url[1024];
#define MB_STRLEN 64
#define NB_SLEN (MB_STRLEN+1)
extern char server_secret[NB_SLEN];
extern char server_motd[NB_SLEN];
extern char server_name[NB_SLEN];
void process_args(int argc,char **argv);
