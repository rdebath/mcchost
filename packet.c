
#include "packet.h"

#if INTERFACE
typedef struct pkt_player_id pkt_player_id;
struct pkt_player_id {
    int protocol;
    char username[NB_SLEN];
    char verify_key[NB_SLEN];
    char unused;
};

typedef struct pkt_setblock pkt_setblock;
struct pkt_setblock {
    struct xyz_t coord;
    int mode;
    block_t block;
};

typedef struct pkt_player_posn pkt_player_posn;
struct pkt_player_posn {
    int player_id;
    struct xyzhv_t player_pos;
};

typedef struct pkt_message pkt_message;
struct pkt_message {
    int msg_flag;
    char message[NB_SLEN];
};
#endif
