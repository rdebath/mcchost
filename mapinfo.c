
#if INTERFACE
#include <stdint.h>
#include <time.h>

#define BLOCKMAX 1024
#define BLOCKNIL ((block_t)-1)
#define MAP_MAGIC	0x1A7FFF00
#define MAP_MAGIC2	0x557FFF00
#define MAP_VERSION	0x00000100
#define World_Pack(x, y, z) (((y) * (uintptr_t)level_prop->cells_z + (z)) * level_prop->cells_x + (x))

typedef uint16_t block_t;

typedef struct xyz_t xyz_t;
struct xyz_t { int x, y, z; };
typedef struct xyzb_t xyzb_t;
struct xyzb_t { uint16_t x, y, z; block_t b; };
typedef struct xyzhv_t xyzhv_t;
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };

typedef struct nbtstr_t nbtstr_t; // This is CP437, NUL terminated.
struct nbtstr_t { char c[NB_SLEN]; };

#define BLK_NUM_TEX	6
#define BLK_NUM_FOG	4
#define BLK_NUM_COORD	6

typedef struct map_info_t map_info_t;
struct map_info_t {
    int magic_no;
    unsigned cells_x;
    unsigned cells_y;
    unsigned cells_z;
    int64_t total_blocks;

    xyzhv_t spawn;

    int version_no;
    int queue_len;
    int last_map_download_size;

    // Do we want/need to backup/save/update ?
    int force_save;
    int readonly;
    int no_unload;
    int disallowchange;	// even if readonly, you can change the working map.
    int reset_hotbar;

    int dirty_save;
    uint32_t metadata_generation;
    time_t time_created;	// Map initial creation time
    time_t last_modified;	// Time of last save to cw file
    time_t last_backup;

    uint8_t uuid[16];
    uint8_t hacks_flags;
    int16_t hacks_jump;

    // Init together til side_offset.
    uint8_t weather;
    int sky_colour;
    int cloud_colour;
    int fog_colour;
    int ambient_colour;
    int sunlight_colour;
    int skybox_colour;

    // EnvMapAppearance
    block_t side_block;
    block_t edge_block;
    int side_level;
    int side_offset;

    // EnvMapAspect properties 0..11
    // block_t side_block;
    // block_t edge_block;
    // int side_level;
    int clouds_height;
    int max_fog;
    int clouds_speed;
    int weather_speed;
    int weather_fade;
    int exp_fog;
    //int side_offset;
    int skybox_hor_speed;
    int skybox_ver_speed;

    int click_distance; // Per level, Per user or both.

    nbtstr_t texname;
    char motd[MB_STRLEN*2+1];

    char name[MB_STRLEN*2+1];
    char software[MB_STRLEN*2+1];
    char theme[MB_STRLEN*2+1];
    char seed[MB_STRLEN*2+1];

    struct blockdef_t blockdef[BLOCKMAX];
    uint8_t block_perms[BLOCKMAX];

    struct selection_cuboid_t cuboid[MAX_CUBES];
    int magic_no2;
};

typedef map_len_t map_len_t;
struct map_len_t {
    int magic_no;
    unsigned cells_x;
    unsigned cells_y;
    unsigned cells_z;
};

#define MAX_CUBES 128

typedef struct selection_cuboid_t selection_cuboid_t;
struct selection_cuboid_t {
    uint8_t defined;
    nbtstr_t name;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t start_z;
    uint16_t end_x;
    uint16_t end_y;
    uint16_t end_z;
    int colour;
    uint8_t opacity;
};

typedef blockdef_t blockdef_t;
struct blockdef_t {

//-------------------------------------------------------------
//----- This section is set by position in default_blocks -----

    nbtstr_t name;

    // BlockDef Packet
    uint8_t collide;
    uint8_t transmits_light;
    uint8_t walksound;
    uint8_t fullbright;
    uint8_t shape;
    uint8_t draw;
    float speed;
    uint16_t textures[BLK_NUM_TEX];
    // CW File: Top, Bottom, Left, Right, Front, Back
    // Packet:  Top, Left, Right, Front, Back, Bottom
    uint8_t fog[BLK_NUM_FOG];
    int8_t coords[BLK_NUM_COORD];
//-------------------------------------------------------------

    // Server only
    block_t fallback;

    // Inventory order packets
    block_t inventory_order;

    // Block permissions (1=NoPlace, 2=NoDelete, 3=Neither)
    uint8_t block_perm;

    uint8_t defined;

    // Physics flags
    uint8_t fire_flag;
    uint8_t door_flag;
    uint8_t mblock_flag;
    uint8_t portal_flag;
    uint8_t lavakills_flag;
    uint8_t waterkills_flag;
    uint8_t tdoor_flag;
    uint8_t rails_flag;
    uint8_t opblock_flag;

    // Physics blocks
    block_t stack_block;
    block_t odoor_block;
    block_t grass_block;
    block_t dirt_block;
};

#define Block_Air 0
#define Block_Stone 1
#define Block_Grass 2
#define Block_Dirt 3
#define Block_Cobble 4
#define Block_Wood 5
#define Block_Sapling 6
#define Block_Bedrock 7
#define Block_ActiveWater 8
#define Block_StillWater 9
#define Block_ActiveLava 10
#define Block_StillLava 11
#define Block_Sand 12
#define Block_Gravel 13
#define Block_CoalOre 16
#define Block_Log 17
#define Block_Leaves 18
#define Block_Sponge 19
#define Block_Glass 20
#define Block_Red 21
#define Block_Orange 22
#define Block_Yellow 23
#define Block_Lime 24
#define Block_Green 25
#define Block_Teal 26
#define Block_Aqua 27
#define Block_Cyan 28
#define Block_Blue 29
#define Block_Pink 33
#define Block_Black 34
#define Block_Grey 35
#define Block_White 36
#define Block_Dandelion 37
#define Block_Rose 38
#define Block_BrownMushroom 39
#define Block_RedMushroom 40
#define Block_Gold 41
#define Block_Iron 42
#define Block_DoubleSlab 43
#define Block_Slab 44
#define Block_Brick 45
#define Block_Bookshelf 47
#define Block_Obsidian 49
#define Block_CobbleSlab 50
#define Block_Rope 51
#define Block_Sandstone 52
#define Block_Snow 53
#define Block_Fire 54
#define Block_LightPink 55
#define Block_ForestGreen 56
#define Block_Brown 57
#define Block_DeepBlue 58
#define Block_Turquoise 59
#define Block_Ice 60
#define Block_CeramicTile 61
#define Block_Magma 62
#define Block_Pillar 63
#define Block_Crate 64
#define Block_StoneBrick 65

#define Block_CP 50
#define Block_CPE 66
#define CPELIMITLO 256
#define CPELIMIT 768

#endif
