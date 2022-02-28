
#if INTERFACE
#include <stdint.h>

#define BLOCKMAX 1024
#define MAP_MAGIC	0x1A7FFF00
#define VALID_MAGIC	0x057FFF00
#define World_Pack(x, y, z) (((y) * (uintptr_t)level_prop->cells_z + (z)) * level_prop->cells_x + (x))

typedef struct xyz_t xyz_t;
struct xyz_t { int x, y, z; };
typedef struct xyzhv_t xyzhv_t;
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };

typedef uint16_t block_t;

typedef struct map_info_t map_info_t;
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


#define Block_Air 0
#define Block_Stone 1
#define Block_Grass 2
#define Block_Dirt 3
#define Block_Cobble 4
#define Block_Wood 5
#define Block_Sapling 6
#define Block_Bedrock 7
#define Block_Water 8
#define Block_StillWater 9
#define Block_ActiveLava 10
#define Block_StillLava 11
#define Block_Sand 12
#define Block_Gravel 13
#define Block_Log 17
#define Block_Leaves 18
#define Block_Sponge 19
#define Block_Glass 20
#define Block_Red 21
#define Block_Cyan 28
#define Block_White 36
#define Block_Dandelion 37
#define Block_Rose 38
#define Block_BrownMushroom 39
#define Block_RedMushroom 40
#define Block_Gold 41
#define Block_DoubleSlab 43
#define Block_Slab 44
#define Block_Bookshelf 47
#define Block_CobbleSlab 50
#define Block_Fire 54
#define Block_LightPink 55
#define Block_Turquoise 59
#define Block_Ice 60
#define Block_Crate 64

#define Block_CPE 50
#define Block_Cust 66

#endif
