/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#define MB_STRLEN 64
#define LOCAL static
#define PKID_OPER       0x0F
void send_op_pkt(int opflg);
#define PKID_DISCON     0x0E
void send_discon_msg_pkt(char *message);
#define PKID_MESSAGE    0x0D
void send_message_pkt(int id,char *message);
#define PKID_DESPAWN    0x0C
void send_despawn_pkt(int player_id);
#define PKID_POSN3      0x0B
#define PKID_POSN       0x08
typedef struct xyzhv_t xyzhv_t;
#include <stdint.h>
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
void send_posn_pkt(xyzhv_t *oldpos,xyzhv_t posn);
#define PKID_SPAWN      0x07
void send_spawn_pkt(int player_id,char *playername,xyzhv_t posn);
#if !defined(FILTER_BLOCKS)
#define block_convert(inblock) (inblock)
#endif
#if defined(FILTER_BLOCKS)
int block_convert(int in);
#endif
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
LOCAL int nb_string_write(uint8_t *pkt,char *str);
#define PKID_IDENT      0x00
void send_server_id_pkt(char *servername,char *servermotd,int user_type);
