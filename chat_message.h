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
void update_chat(pkt_message *pkt);
void send_msg_pkt_filtered(int msg_flag,char *message);
void write_logfile(char *str,int len);
#define PKBUF		8192
void post_chat(int where,char *chat,int chat_len);
extern char user_id[NB_SLEN];
void convert_chat_message(char *msg);
void run_command(char *msg);
void process_chat_message(int msg_flag,char *msg);
