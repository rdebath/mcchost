/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#define PKID_POSN2      0x0A
#define PKID_POSN1      0x09
#define PKID_SRVBLOCK   0x06
#define PKID_SETBLOCK   0x05
#define INTERFACE 0
#define MB_STRLEN 64
#define LOCAL static
#define PKID_OPER       0x0F
extern int ofd;
void send_op_pkt(int ofd,int opflg);
#define PKID_DISCON     0x0E
void send_discon_msg_pkt(int ofd,char *message);
#define PKID_MESSAGE    0x0D
void send_message_pkt(int ofd,int player_id,char *message);
#define PKID_DESPAWN    0x0C
void send_despawn_pkt(int ofd,int player_id);
#define PKID_POSN3      0x0B
#define PKID_POSN0      0x08
typedef struct xyzhv_t xyzhv_t;
#include <stdint.h>
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
void send_posn_pkt(int ofd,xyzhv_t *oldpos,xyzhv_t posn);
#define PKID_SPAWN      0x07
void send_spawn_pkt(int ofd,int player_id,char *playername,xyzhv_t posn);
#define PKID_LVLDONE    0x04
void send_lvldone_pkt(int ofd,int x,int y,int z);
#define PKID_LVLDATA    0x03
void send_lvldata_pkt(int ofd,char *block,int len,int percent);
#define PKID_LVLINIT    0x02
void send_lvlinit_pkt(int ofd);
#define PKID_PING       0x01
void send_ping_pkt(int ofd);
LOCAL int nb_string_write(uint8_t *pkt,char *str);
#define PKID_IDENT      0x00
void send_server_id_pkt(int ofd,char *servername,char *servermotd);
