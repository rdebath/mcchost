/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void complete_connection();
#include <stdint.h>
typedef uint16_t block_t;
extern block_t max_blockno_to_send;
typedef struct pkt_extentry pkt_extentry;
#define MB_STRLEN 64
#define NB_SLEN (MB_STRLEN+1)
struct pkt_extentry {
    char extname[NB_SLEN];
    int version;
};
void process_extentry(pkt_extentry *pkt);
#define IntBE32(x)  (*(uint8_t*)((x)+3) + (*(uint8_t*)((x)+2)<<8) + \
                    (*(uint8_t*)((x)+1)<<16) + (*(uint8_t*)((x)+0)<<24))
extern int cpe_extn_remaining;
extern char client_software[NB_SLEN];
void process_chat_message(int msg_flag,char *msg);
void cpy_nbstring(char *buf,char *str);
typedef struct pkt_message pkt_message;
struct pkt_message {
    int msg_flag;
    char message[NB_SLEN];
};
#define PKID_MESSAGE    0x0D
typedef struct pkt_player_posn pkt_player_posn;
typedef struct xyzhv_t xyzhv_t;
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
struct pkt_player_posn {
    int player_id;
    struct xyzhv_t pos;
};
void update_player_pos(pkt_player_posn pkt);
#define PKID_POSN       0x08
typedef struct pkt_setblock pkt_setblock;
typedef struct xyz_t xyz_t;
struct xyz_t { int x, y, z; };
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
#define PKID_OPER       0x0F
extern int ignore_cpe;
#define PKID_CUSTBLOCK	0x13	/*ClientSend&Rcv*/
#define PKID_EXTENTRY   0x11    /*ClientSend&Rcv*/
#define PKID_EXTINFO    0x10    /*ClientSend&Rcv*/
void process_client_message(int cmd,char *pktbuf);
extern int msglen[256];
void send_queued_blocks();
void send_queued_chats();
void check_user();
void send_ping_pkt();
extern int cpe_pending;
void remote_received(char *str,int len);
void on_select_timeout();
extern int line_ifd;
extern int line_ofd;
void flush_to_remote();
void fatal(char *emsg);
void write_to_remote(char *str,int len);
int bytes_queued_to_send();
int do_select();
void run_request_loop();
extern time_t last_ping;
extern int pending_marks;
extern int in_rcvd;
#define PKBUF		8192
extern int64_t bytes_sent;
