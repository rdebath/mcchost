/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
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
#define INTERFACE 0
