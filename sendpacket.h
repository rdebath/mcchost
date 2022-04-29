/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#define PKID_CUSTBLOCK	0x13	/*ClientSend&Rcv*/
void send_customblocks_pkt();
#define PKID_CLICKDIST	0x12
void send_clickdistance_pkt(int dist);
#define PKID_EXTENTRY   0x11    /*ClientSend&Rcv*/
void send_extentry_pkt(char *extension,int version);
#define PKID_EXTINFO    0x10    /*ClientSend&Rcv*/
void send_extinfo_pkt(char *appname,int num_extn);
#define PKID_OPER       0x0F
void send_op_pkt(int opflg);
#define PKID_DISCON     0x0E
void send_discon_msg_pkt(char *message);
#define PKID_MESSAGE    0x0D
void send_message_pkt(int id,char *message);
#define PKID_DESPAWN    0x0C
void send_despawn_pkt(int player_id);
#define PKID_POSN3      0x0B
#define PKID_POSN2      0x0A
#define PKID_POSN1      0x09
#define PKID_POSN       0x08
typedef struct xyzhv_t xyzhv_t;
#include <stdint.h>
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
void send_posn_pkt(int player_id,xyzhv_t *oldpos,xyzhv_t posn);
#define PKID_SPAWN      0x07
void send_spawn_pkt(int player_id,char *playername,xyzhv_t posn);
typedef uint16_t block_t;
extern block_t max_blockno_to_send;
block_t f_block_convert(block_t in);
#define block_convert(_bl) ((_bl)<=max_blockno_to_send?_bl:f_block_convert(_bl))
#define PKID_SRVBLOCK   0x06
void send_setblock_pkt(int x,int y,int z,int block);
#define PKID_LVLDONE    0x04
void send_lvldone_pkt(int x,int y,int z);
#define PKID_LVLDATA    0x03
void send_lvldata_pkt(char *block,int len,int percent);
#define PKID_LVLINIT    0x02
void send_lvlinit_pkt();
#define PKID_PING       0x01
void send_ping_pkt();
void write_to_remote(char *str,int len);
#define PKID_IDENT      0x00
void send_server_id_pkt(char *servername,char *servermotd,int user_type);
#define MB_STRLEN 64
#define LOCAL static
LOCAL int nb_string_write(uint8_t *pkt,char *str);
