#include "packet.h"

#if INTERFACE
#include <stdint.h>

#define MB_STRLEN 64
#define NB_SLEN (MB_STRLEN+1)

#define IntBE16(x) ((int16_t)(*(uint8_t*)((x)+1) + *(uint8_t*)(x)*256))
#define UIntBE16(x) ((uint16_t)(*(uint8_t*)((x)+1) + *(uint8_t*)(x)*256))
#define UIntLE16(x) (*(uint8_t*)(x) + *(uint8_t*)((x)+1)*256)
#define IntLE32(x)  (*(uint8_t*)(x) + (*(uint8_t*)((x)+1)<<8) + \
                    (*(uint8_t*)((x)+2)<<16) + (*(uint8_t*)((x)+3)<<24))
#define IntBE32(x)  (*(uint8_t*)((x)+3) + (*(uint8_t*)((x)+2)<<8) + \
                    (*(uint8_t*)((x)+1)<<16) + (*(uint8_t*)((x)+0)<<24))
#define SB(x)      ((int)((int8_t)(x)))
#define UB(x)      ((int)((uint8_t)(x)))

// Limit the position to 21 bits, that is +/-32768 cells.
#define BIT20      0x100000
#define SBIT21(X)  (((X&(BIT20*2-1))^BIT20)-BIT20)

/* Packet ID numbers.
 */
#define PKBUF		8192
#define PKID_IDENT      0x00
#define PKID_PING       0x01
#define PKID_LVLINIT    0x02
#define PKID_LVLDATA    0x03
#define PKID_LVLDONE    0x04
#define PKID_SETBLOCK   0x05
#define PKID_SRVBLOCK   0x06
#define PKID_SPAWN      0x07
#define PKID_POSN       0x08
#define PKID_POSN1      0x09
#define PKID_POSN2      0x0A
#define PKID_POSN3      0x0B
#define PKID_DESPAWN    0x0C
#define PKID_MESSAGE    0x0D
#define PKID_DISCON     0x0E
#define PKID_OPER       0x0F
#define PKID_EXTINFO    0x10    /*ClientSend&Rcv*/
#define PKID_EXTENTRY   0x11    /*ClientSend&Rcv*/
#define PKID_CLICKDIST	0x12
#define PKID_CUSTBLOCK	0x13	/*ClientSend&Rcv*/
#define PKID_HELDBLOCK	0x14
#define PKID_PLAYERNAME	0x16
#define PKID_ADDENTv1	0x17
#define PKID_RMPLAYER	0x18
#define PKID_MAPCOLOUR	0x19
#define PKID_BLOCKPERM	0x1c
#define PKID_MAPAPPEAR	0x1e
#define PKID_WEATHER	0x1f
#define PKID_ADDENT	0x21
#define PKID_PLAYERCLK	0x22	/*ClientSend*/
#define PKID_BLOCKDEF	0x23
#define PKID_BLOCKUNDEF	0x24
#define PKID_BLOCKDEF2	0x25
#define PKID_TEXTCOLOUR 0x27
#define PKID_TEXURL	0x28
#define PKID_MAPPROP	0x29
#define PKID_INVORDER	0x2c
#define PKID_HOTBAR     0x2d
#define PKID_SETSPAWN	0x2e

/* These are the structures for received packets after conversion from
 * the line format.
 */
typedef struct pkt_player_id pkt_player_id;
struct pkt_player_id {
    int protocol;
    char user_id[NB_SLEN];
    char mppass[NB_SLEN];
    char cpe_flag;
};

typedef struct pkt_setblock pkt_setblock;
struct pkt_setblock {
    struct xyz_t coord;
    int mode;
    block_t block;
    block_t heldblock;
};

typedef struct pkt_player_posn pkt_player_posn;
struct pkt_player_posn {
    uint8_t player_id;
    block_t held_block;
    struct xyzhv_t pos;
};

typedef struct pkt_message pkt_message;
struct pkt_message {
    uint8_t player_id;
    uint8_t message_type;
    char message[NB_SLEN];
};

typedef struct pkt_extentry pkt_extentry;
struct pkt_extentry {
    char extname[NB_SLEN];
    int version;
};

typedef pkt_playerclick pkt_playerclick;
struct pkt_playerclick {
    uint8_t button;
    uint8_t action;
    int16_t h;
    int16_t v;
    int entity;
    int block_x;
    int block_y;
    int block_z;
    uint8_t face;
}
#endif



/* Message packet sizes, send and receive use the same list.
 * Note that the protocol does NOT include length fields.
 *
 * This is all defined base and CPE packets, but I haven't named them all.
 */
int msglen[256] = {
#define PKID_IDENT	0x00	/*ClientSend&Rcv*/
    /* 0x00 */ 131,
#define PKID_PING	0x01
    /* 0x01 */ 1,
#define PKID_LVLINIT	0x02
    /* 0x02 */ 1,
#define PKID_LVLDATA	0x03
    /* 0x03 */ 1024+4,
#define PKID_LVLDONE	0x04
    /* 0x04 */ 7,
#define PKID_SETBLOCK	0x05	/*ClientSend*/
    /* 0x05 */ 9,
#define PKID_SRVBLOCK	0x06
    /* 0x06 */ 8,
#define PKID_SPAWN	0x07
    /* 0x07 */ 2+64+6+2,
#define PKID_POSN	0x08	/*ClientSend as PKID_POSN */
    /* 0x08 */ 2+6+2,
#define PKID_POSN1	0x09
    /* 0x09 */ 7,
#define PKID_POSN2	0x0A
    /* 0x0a */ 5,
#define PKID_POSN3	0x0B
    /* 0x0b */ 4,
#define PKID_DESPAWN	0x0C
    /* 0x0c */ 2,
#define PKID_MESSAGE	0x0D	/*ClientSend*/
    /* 0x0d */ 2+64,
#define PKID_DISCON	0x0E
    /* 0x0e */ 1+64,
#define PKID_OPER	0x0F
    /* 0x0f */ 2,
#define PKID_EXTINFO	0x10	/*ClientSend&Rcv*/
    /* 0x10 */ 67,
#define PKID_EXTENTRY	0x11	/*ClientSend&Rcv*/
    /* 0x11 */ 69,
#define PKID_CLICKDIST	0x12
    /* 0x12 */ 3,
#define PKID_CUSTBLOCK	0x13	/*ClientSend&Rcv*/
    /* 0x13 */ 2,
#define PKID_HELDBLOCK	0x14
    /* 0x14 */ 3,
#define PKID_SETHOTKEY	0x15
    /* 0x15 */ 134,
#define PKID_PLAYERNAME	0x16
    /* 0x16 */ 196,
#define PKID_ADDENTv1	0x17
    /* 0x17 */ 130,
#define PKID_RMPLAYER	0x18
    /* 0x18 */ 3,
#define PKID_MAPCOLOUR	0x19
    /* 0x19 */ 8,
#define PKID_ADDZONE	0x1a
    /* 0x1a */ 86,
#define PKID_RMZONE	0x1b
    /* 0x1b */ 2,
#define PKID_BLOCKPERM	0x1c
    /* 0x1c */ 4,
#define PKID_CHANGEMDL	0x1d
    /* 0x1d */ 66,
#define PKID_MAPAPPEAR	0x1e
    /* 0x1e */ 0, // 69, 73
#define PKID_WEATHER	0x1f
    /* 0x1f */ 2,
#define PKID_HACKCTL	0x20
    /* 0x20 */ 8,
#define PKID_ADDENT	0x21
    /* 0x21 */ 138,
#define PKID_PLAYERCLK	0x22	/*ClientSend*/
    /* 0x22 */ 15,
#define PKID_BLOCKDEF	0x23
    /* 0x23 */ 80,
#define PKID_BLOCKUNDEF	0x24
    /* 0x24 */ 2,
#define PKID_BLOCKDEF2	0x25
    /* 0x25 */ 88,
#define PKID_BULKUPDATE	0x26
    /* 0x26 */ 1282,
#define PKID_TEXTCOLOUR	0x27
    /* 0x27 */ 6,
#define PKID_TEXURL	0x28
    /* 0x28 */ 65,
#define PKID_MAPPROP	0x29
    /* 0x29 */ 6,
#define PKID_ENTITYPROP	0x2a
    /* 0x2a */ 7,
#define PKID_PINGPONG	0x2b	/*ClientSend&Rcv*/
    /* 0x2b */ 4,
#define PKID_INVORDER	0x2c
    /* 0x2c */ 3,
#define PKID_HOTBAR	0x2d
    /* 0x2d */ 3,
#define PKID_SETSPAWN	0x2e
    /* 0x2e */ 9,
    /* 0x2f */ 16,
    /* 0x30 */ 36,
    /* 0x31 */ 26,

    0
};
