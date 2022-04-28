/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void send_queued_chats();
typedef struct pkt_message pkt_message;
#define MB_STRLEN 64
#define NB_SLEN (MB_STRLEN+1)
struct pkt_message {
    int msg_flag;
    char message[NB_SLEN];
};
void update_chat(pkt_message pkt);
void write_logfile(char *str,int len);
#define PKBUF		8192
void post_chat(char *chat,int chat_len);
extern char user_id[NB_SLEN];
void run_command(char *msg);
void convert_chat_message(int msg_flag,char *msg);
