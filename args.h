/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
extern int enable_cp437;
#include <stdint.h>
typedef uint16_t block_t;
extern block_t max_blockno_to_send;
extern int server_private;
extern int start_tcp_server;
void open_logfile(char *logfile_name,int logfile_raw_p);
extern int tcp_port_no;
extern int enable_heartbeat_poll;
extern char heartbeat_url[1024];
#define MB_STRLEN 64
#define NB_SLEN	(MB_STRLEN+1)
extern char server_salt[NB_SLEN];
extern char server_motd[NB_SLEN];
extern char server_name[NB_SLEN];
void process_args(int argc,char **argv);
