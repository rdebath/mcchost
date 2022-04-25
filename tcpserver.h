/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#define MB_STRLEN 64
#define NB_SLEN	(MB_STRLEN+1)
extern char server_name[NB_SLEN];
#define LOCAL static
LOCAL char *quoteurl(char *s,char *dest,int len);
int current_user_count();
extern int server_private;
extern char heartbeat_url[1024];
extern char server_salt[NB_SLEN];
void delete_session_id(int pid);
LOCAL void send_heartbeat_poll();
extern int enable_heartbeat_poll;
LOCAL void cleanup_zombies();
void process_connection();
extern int line_ifd;
extern int line_ofd;
LOCAL int accept_new_connection();
extern int tcp_port_no;
LOCAL int start_listen_socket(char *listen_addr,int port);
void tcpserver();
