/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdint.h>
#define Block_Cust 66
#define Block_CPE 50
#define Block_StoneBrick 65
#define Block_Crate 64
#define Block_Pillar 63
#define Block_Magma 62
#define Block_CeramicTile 61
#define Block_Ice 60
#define Block_Turquoise 59
#define Block_DeepBlue 58
#define Block_Brown 57
#define Block_ForestGreen 56
#define Block_LightPink 55
#define Block_Fire 54
#define Block_Snow 53
#define Block_Sandstone 52
#define Block_Rope 51
#define Block_CobbleSlab 50
#define Block_Obsidian 49
#define Block_Bookshelf 47
#define Block_Slab 44
#define Block_DoubleSlab 43
#define Block_Iron 42
#define Block_Gold 41
#define Block_RedMushroom 40
#define Block_BrownMushroom 39
#define Block_Rose 38
#define Block_Dandelion 37
#define Block_White 36
#define Block_Pink 33
#define Block_Blue 29
#define Block_Cyan 28
#define Block_Green 25
#define Block_Orange 22
#define Block_Red 21
#define Block_Glass 20
#define Block_Sponge 19
#define Block_Leaves 18
#define Block_Log 17
#define Block_Gravel 13
#define Block_Sand 12
#define Block_StillLava 11
#define Block_ActiveLava 10
#define Block_StillWater 9
#define Block_Water 8
#define Block_Bedrock 7
#define Block_Sapling 6
#define Block_Wood 5
#define Block_Cobble 4
#define Block_Dirt 3
#define Block_Grass 2
#define Block_Stone 1
#define Block_Air 0
typedef struct map_len_t map_len_t;
struct map_len_t {
    int magic_no;
    unsigned cells_x;
    unsigned cells_y;
    unsigned cells_z;
};
typedef struct blockdef_t blockdef_t;
#define MB_STRLEN 64
#define NB_SLEN (MB_STRLEN+1)
#define BLK_NUM_TEX	6
#define BLK_NUM_FOG	4
#define BLK_NUM_COORD	6
typedef uint16_t block_t;
struct blockdef_t {
    char name[NB_SLEN];

    uint8_t collide;
    uint8_t transparent;
    uint8_t walksound;
    uint8_t blockslight;
    uint8_t shape;
    uint8_t draw;
    float speed;
    uint16_t textures[BLK_NUM_TEX];
    uint8_t fog[BLK_NUM_FOG];
    int8_t cords[BLK_NUM_COORD];

    block_t fallback;
    block_t inventory_order;

    uint8_t defined;

    uint8_t fire_flag;
    uint8_t door_flag;
    uint8_t mblock_flag;
    uint8_t portal_flag;
    uint8_t lavakills_flag;
    uint8_t waterkills_flag;
    uint8_t tdoor_flag;
    uint8_t rails_flag;
    uint8_t opblock_flag;

    block_t stack_block;
    block_t odoor_block;
    block_t grass_block;
    block_t dirt_block;
};
typedef struct map_info_t map_info_t;
typedef struct xyzhv_t xyzhv_t;
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
#define BLOCKMAX 1024
struct map_info_t {
    int magic_no;
    unsigned cells_x;
    unsigned cells_y;
    unsigned cells_z;
    int64_t total_blocks;

    xyzhv_t spawn;

    int hacks_flags;
    int queue_len;
    int last_map_download_size;

    // Init together til side_offset.
    uint8_t weather;
    int sky_colour;
    int cloud_colour;
    int fog_colour;
    int ambient_colour;
    int sunlight_colour;

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

    char texname[NB_SLEN];
    char motd[NB_SLEN];

    struct blockdef_t blockdef[BLOCKMAX];
    int invt_order[BLOCKMAX];
    uint8_t block_perms[BLOCKMAX];

    int version_no;
};
typedef struct xyzb_t xyzb_t;
struct xyzb_t { uint16_t x, y, z, b; };
typedef struct xyz_t xyz_t;
struct xyz_t { int x, y, z; };
typedef struct shared_data_t shared_data_t;
typedef struct block_queue_t block_queue_t;
struct block_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    uint32_t curr_offset;
    uint32_t queue_len;

    xyzb_t updates[1];
};
typedef struct chat_queue_t chat_queue_t;
typedef struct chat_entry_t chat_entry_t;
typedef struct pkt_message pkt_message;
struct pkt_message {
    int msg_flag;
    char message[NB_SLEN];
};
struct chat_entry_t {
    int to_level_id;
    int to_player_id;
    int to_team_id;
    pkt_message msg;
};
struct chat_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    uint32_t curr_offset;
    uint32_t queue_len;

    chat_entry_t updates[1];
};
typedef struct client_data_t client_data_t;
typedef struct client_entry_t client_entry_t;
extern char client_software[NB_SLEN];
struct client_entry_t {
    char name[NB_SLEN];
    char client_software[NB_SLEN];
    xyzhv_t posn;
    uint8_t active;
    pid_t session_id;
};
#define MAX_USER	255
struct client_data_t {
    int magic1;
    uint32_t generation;
    client_entry_t user[MAX_USER];
    int magic2;
};
typedef struct shmem_t shmem_t;
struct shmem_t {
    void * ptr;
    intptr_t len;
    int lock_fd;
};
#define SHMID_COUNT	5
struct shared_data_t {
    volatile map_info_t *prop;
    volatile block_t *blocks;
    volatile block_queue_t* blockq;
    volatile chat_queue_t *chat;
    volatile client_data_t *client;

    shmem_t dat[SHMID_COUNT];
};
extern struct shared_data_t shdat;
#define level_prop shdat.prop
#define World_Pack(x, y, z) (((y) * (uintptr_t)level_prop->cells_z + (z)) * level_prop->cells_x + (x))
#define VALID_MAGIC	0x057FFF00
#define MAP_VERSION	0xA0000100
#define MAP_MAGIC	0x1A7FFF00
#define INTERFACE 0
