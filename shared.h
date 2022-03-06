/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void fatal(char *emsg);
void fatal(char *emsg);
#define LOCAL static
void set_last_chat_queue_id();
typedef struct chat_queue_t chat_queue_t;
typedef struct chat_entry_t chat_entry_t;
typedef struct pkt_message pkt_message;
#define MB_STRLEN 64
#define NB_SLEN	(MB_STRLEN+1)
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
    int curr_offset;
    int queue_len;

    chat_entry_t updates[1];
};
extern volatile chat_queue_t *level_chat_queue;
extern intptr_t level_chat_queue_len;
void wipe_last_chat_queue_id();
#if !defined(_REENTRANT)
#define USE_FCNTL
#endif
#if !defined(USE_FCNTL)
#include <semaphore.h>
#endif
void stop_chat_queue();
void create_chat_queue();
typedef struct xyzb_t xyzb_t;
#include <stdint.h>
struct xyzb_t { uint16_t x, y, z, b; };
extern intptr_t level_block_queue_len;
void wipe_last_block_queue_id();
#define PKID_SRVBLOCK   0x06
#define PKID_SRVBLOCK   0x06
extern int msglen[256];
void create_block_queue(char *levelname);
typedef struct block_queue_t block_queue_t;
struct block_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    int curr_offset;
    int queue_len;

    xyzb_t updates[1];
};
extern volatile block_queue_t *level_block_queue;
#define VALID_MAGIC	0x057FFF00
#define Block_Dirt 3
#define Block_Grass 2
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
#define MAP_MAGIC	0x1A7FFF00
LOCAL void *allocate_shared(char *share_name,int share_size,intptr_t *shared_len);
void stop_block_queue();
void stop_shared(void);
void start_shared(char *levelname);
extern intptr_t level_blocks_len;
typedef uint16_t block_t;
extern volatile block_t *level_blocks;
extern intptr_t level_prop_len;
#if !(defined(USE_FCNTL))
extern sem_t *shared_global_mutex;
#endif
#if !defined(USE_FCNTL)
#define unlock_shared() sem_post(shared_global_mutex);
#endif
#if defined(USE_FCNTL)
void unlock_shared(void);
#endif
#if !defined(USE_FCNTL)
#define lock_shared() sem_wait(shared_global_mutex);
#endif
#if defined(USE_FCNTL)
void lock_shared(void);
#endif
#define INTERFACE 0
