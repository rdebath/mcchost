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
void cpy_nbstring(char *buf,char *str);
void cpy_nbstring(char *buf,char *str);
#define PKID_MESSAGE    0x0D
#define PKID_MESSAGE    0x0D
typedef struct pkt_setblock pkt_setblock;
typedef struct xyz_t xyz_t;
#include <stdint.h>
struct xyz_t { int x, y, z; };
typedef uint16_t block_t;
struct pkt_setblock {
    struct xyz_t coord;
    int mode;
    block_t block;
    block_t heldblock;
};
void update_block(pkt_setblock pkt);
#define Block_Air 0
#define IntBE16(x) ((int16_t)(*(uint8_t*)((x)+1) + *(uint8_t*)(x)*256))
#define PKID_SETBLOCK   0x05
#define PKID_SETBLOCK   0x05
void process_client_message(int cmd,char *pktbuf);
extern int msglen[256];
void send_queued_blocks();
void send_queued_chats();
void remote_received(char *str,int len);
void on_select_timeout();
extern int line_ofd;
extern int line_ifd;
void fatal(char *emsg);
void fatal(char *emsg);
void write_to_remote(char *str,int len);
int bytes_queued_to_send();
int do_select();
void run_request_loop();
extern int pending_marks;
extern int in_rcvd;
#define PKBUF		8192
extern int64_t bytes_sent;
