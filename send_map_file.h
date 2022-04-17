/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void send_lvldone_pkt(int x,int y,int z);
void send_lvldata_pkt(char *block,int len,int percent);
#include <stdint.h>
typedef uint16_t block_t;
extern volatile block_t *level_blocks;
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
void send_lvlinit_pkt();
void set_last_block_queue_id();
void send_map_file();
extern block_t cpe_conv[];
#if !defined(FILTER_BLOCKS)
#define block_convert(inblock) (inblock)
#endif
#if defined(FILTER_BLOCKS)
int block_convert(int in);
#endif
#define INTERFACE 0
