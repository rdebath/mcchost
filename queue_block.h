/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#if !defined(FILTER_BLOCKS)
#define block_convert(inblock) (inblock)
#endif
#if defined(FILTER_BLOCKS)
int block_convert(int in);
#endif
void send_setblock_pkt(int x,int y,int z,int block);
void send_map_file();
int bytes_queued_to_send();
void send_queued_blocks();
void wipe_last_block_queue_id();
extern char *level_name;
void create_block_queue(char *levelname);
void set_last_block_queue_id();
void unlock_shared(void);
void lock_shared(void);
#include <stdint.h>
#define Block_Air 0
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

    int hacks_flags;
    int queue_len;
    int last_map_download_size;

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

};
extern volatile map_info_t *level_prop;
#define World_Pack(x, y, z) (((y) * (uintptr_t)level_prop->cells_z + (z)) * level_prop->cells_x + (x))
typedef uint16_t block_t;
extern volatile block_t *level_blocks;
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
extern intptr_t level_block_queue_len;
typedef struct block_queue_t block_queue_t;
typedef struct xyzb_t xyzb_t;
struct xyzb_t { uint16_t x, y, z, b; };
struct block_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    int curr_offset;
    int queue_len;

    xyzb_t updates[1];
};
extern volatile block_queue_t *level_block_queue;
#define INTERFACE 0
