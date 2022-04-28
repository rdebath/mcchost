/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void send_message_pkt(int id,char *message);
extern int enable_cp437;
void send_queued_chats();
void wipe_last_chat_queue_id();
void set_last_chat_queue_id();
void unlock_chat_shared(void);
void lock_chat_shared(void);
void create_chat_queue();
typedef struct shared_data_t shared_data_t;
typedef struct map_info_t map_info_t;
#include <stdint.h>
typedef struct xyzhv_t xyzhv_t;
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
typedef uint16_t block_t;
#define MB_STRLEN 64
#define NB_SLEN (MB_STRLEN+1)
typedef struct blockdef_t blockdef_t;
#define BLK_NUM_TEX	6
#define BLK_NUM_FOG	4
#define BLK_NUM_COORD	6
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
typedef struct block_queue_t block_queue_t;
typedef struct xyzb_t xyzb_t;
struct xyzb_t { uint16_t x, y, z, b; };
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
struct client_entry_t {
    char name[65];
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
#define level_chat_queue shdat.chat
void update_chat(pkt_message pkt);
#define INTERFACE 0
