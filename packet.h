/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
extern int msglen[256];
typedef struct pkt_message pkt_message;
#define MB_STRLEN 64
#define NB_SLEN	(MB_STRLEN+1)
struct pkt_message {
    int msg_flag;
    char message[NB_SLEN];
};
typedef struct xyzhv_t xyzhv_t;
#include <stdint.h>
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
typedef struct pkt_player_posn pkt_player_posn;
struct pkt_player_posn {
    int player_id;
    struct xyzhv_t player_pos;
};
typedef uint16_t block_t;
typedef struct xyz_t xyz_t;
struct xyz_t { int x, y, z; };
typedef struct pkt_setblock pkt_setblock;
struct pkt_setblock {
    struct xyz_t coord;
    int mode;
    block_t block;
};
typedef struct pkt_player_id pkt_player_id;
struct pkt_player_id {
    int protocol;
    char username[NB_SLEN];
    char verify_key[NB_SLEN];
    char unused;
};
#define PKID_OPER       0x0F
#define PKID_OPER       0x0F
#define PKID_DISCON     0x0E
#define PKID_DISCON     0x0E
#define PKID_MESSAGE    0x0D
#define PKID_MESSAGE    0x0D
#define PKID_DESPAWN    0x0C
#define PKID_DESPAWN    0x0C
#define PKID_POSN3      0x0B
#define PKID_POSN3      0x0B
#define PKID_POSN2      0x0A
#define PKID_POSN2      0x0A
#define PKID_POSN1      0x09
#define PKID_POSN1      0x09
#define PKID_POSN0      0x08
#define PKID_POSN0      0x08
#define PKID_SPAWN      0x07
#define PKID_SPAWN      0x07
#define PKID_SRVBLOCK   0x06
#define PKID_SRVBLOCK   0x06
#define PKID_SETBLOCK   0x05
#define PKID_SETBLOCK   0x05
#define PKID_LVLDONE    0x04
#define PKID_LVLDONE    0x04
#define PKID_LVLDATA    0x03
#define PKID_LVLDATA    0x03
#define PKID_LVLINIT    0x02
#define PKID_LVLINIT    0x02
#define PKID_PING       0x01
#define PKID_PING       0x01
#define PKID_IDENT      0x00
#define PKID_IDENT      0x00
#define PKBUF		8192
#define BIT20      0x100000
#define SBIT21(X)  (((X&(BIT20*2-1))^BIT20)-BIT20)
#define UB(x)      ((int)((uint8_t)(x)))
#define SB(x)      ((int)((int8_t)(x)))
#define IntBE32(x)  (*(uint8_t*)((x)+3) + (*(uint8_t*)((x)+2)<<8) + \
                    (*(uint8_t*)((x)+1)<<16) + (*(uint8_t*)((x)+0)<<24))
#define IntLE32(x)  (*(uint8_t*)(x) + (*(uint8_t*)((x)+1)<<8) + \
                    (*(uint8_t*)((x)+2)<<16) + (*(uint8_t*)((x)+3)<<24))
#define UIntLE16(x) (*(uint8_t*)(x) + *(uint8_t*)((x)+1)*256)
#define UIntBE16(x) ((uint16_t)(*(uint8_t*)((x)+1) + *(uint8_t*)(x)*256))
#define IntBE16(x) ((int16_t)(*(uint8_t*)((x)+1) + *(uint8_t*)(x)*256))
#define INTERFACE 0
