/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdint.h>
#define Block_Air 0
#if !defined(_REENTRANT)
#define USE_FCNTL
#endif
#if !defined(USE_FCNTL)
#include <semaphore.h>
#endif
typedef struct map_info_t map_info_t;
typedef struct xyzhv_t xyzhv_t;
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
typedef struct block_defn block_defn;
struct block_defn {
    char * name;

    int collide;
    int transparent;
    int walksound;
    int light;
    int shape;
    int draw;
    float speed;
    int textures[6];
    uint8_t fog[4];
    int8_t cords[6];

    int fallback;
    int inventory_order;

    int defined;
    int changed;
    int dup_name;
    int fix_name;
    int used;

    int door_flag;
    int mblock_flag;
    int portal_flag;
    int lava_flag;
    int water_flag;
    int tdoor_flag;
    int rails_flag;
    int opblock_flag;

    int stack_block;
    int odoor_block;
    int grass_block;
    int dirt_block;
};
#define BLOCKMAX 1024
struct map_info_t {
    int magic_no;
    int version_no;
    unsigned int cells_x;
    unsigned int cells_y;
    unsigned int cells_z;
    int valid_blocks;

    xyzhv_t spawn;

    // Init together til side_level.
    int weather;
    int sky_colour;
    int cloud_colour;
    int fog_colour;
    int ambient_colour;
    int sunlight_colour;
    int side_block;
    int edge_block;
    int side_level;

    char texname[65];
    char motd[65];

    struct block_defn blockdef[BLOCKMAX];
    int invt_order[BLOCKMAX];

    unsigned char block_perms[BLOCKMAX];
    int hacks_flags;

};
extern volatile map_info_t *level_prop;
#define World_Pack(x, y, z) (((y) * (uintptr_t)level_prop->cells_z + (z)) * level_prop->cells_x + (x))
typedef uint16_t block_t;
extern volatile block_t *level_blocks;
#define IntBE16(x) ((int16_t)(*(uint8_t*)((x)+1) + *(uint8_t*)(x)*256))
typedef struct pkt_setblock pkt_setblock;
typedef struct xyz_t xyz_t;
struct xyz_t { int x, y, z; };
struct pkt_setblock {
    struct xyz_t coord;
    int mode;
    block_t block;
};
#define PKID_SETBLOCK   0x05
#define PKID_SETBLOCK   0x05
void process_client_message(int cmd,char *pkt,int len);
extern int msglen[256];
void remote_received(char *str,int len);
void on_select_timeout();
extern int line_ofd;
extern int line_ifd;
void fatal(char *emsg);
void fatal(char *emsg);
void write_to_remote(char *str,int len);
int do_select();
void run_request_loop();
extern int in_rcvd;
#define PKBUF		8192
extern int64_t bytes_sent;
