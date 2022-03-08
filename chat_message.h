/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
typedef struct pkt_message pkt_message;
#define MB_STRLEN 64
#define NB_SLEN	(MB_STRLEN+1)
struct pkt_message {
    int msg_flag;
    char message[NB_SLEN];
};
void update_chat(pkt_message pkt);
void post_chat(char *chat,int chat_len);
#define PKBUF		8192
extern char user_id[NB_SLEN];
void send_message_pkt(int id,char *message);
void convert_chat_message(pkt_message pkt);
