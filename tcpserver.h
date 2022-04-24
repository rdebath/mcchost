/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#define LOCAL static
void delete_session_id(int pid);
void process_connection();
extern int line_ifd;
extern int line_ofd;
LOCAL int accept_new_connection();
extern int tcp_port_no;
LOCAL int start_listen_socket(char *listen_addr,int port);
void tcpserver();
