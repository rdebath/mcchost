/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#define LOCAL static
LOCAL void share_lock(int fd,int mode,int l_type);
void fatal(char *emsg);
void fatal(char *emsg);
void set_last_chat_queue_id();
void unlock_chat_shared(void);
void lock_chat_shared(void);
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
void wipe_last_chat_queue_id();
void stop_chat_queue();
void create_chat_queue();
#define MAGIC_USR	0x0012FF7E
void stop_client_list();
void open_client_list();
typedef struct xyzb_t xyzb_t;
#include <stdint.h>
struct xyzb_t { uint16_t x, y, z, b; };
void wipe_last_block_queue_id();
#define PKID_SRVBLOCK   0x06
extern int msglen[256];
void create_block_queue(char *levelname);
void unlock_shared(void);
void open_blocks(char *levelname);
void createmap(char *levelname);
#define MAP_MAGIC	0x1A7FFF00
#define MAP_VERSION	0xA0000100
void lock_shared(void);
typedef struct shmem_t shmem_t;
struct shmem_t {
    void * ptr;
    intptr_t len;
    int lock_fd;
};
LOCAL void allocate_shared(char *share_name,int share_size,shmem_t *shm);
void stop_block_queue();
void stop_shared(void);
void open_level_files(char *levelname);
typedef struct shared_data_t shared_data_t;
typedef struct map_info_t map_info_t;
typedef struct xyzhv_t xyzhv_t;
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
typedef uint16_t block_t;
typedef struct block_defn block_defn;
struct block_defn {
    char name[64];

    uint8_t collide;
    uint8_t transparent;
    uint8_t walksound;
    uint8_t blockslight;
    uint8_t shape;
    uint8_t draw;
    float speed;
    uint16_t textures[6];
    uint8_t fog[4];
    int8_t cords[6];

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
    unsigned int cells_x;
    unsigned int cells_y;
    unsigned int cells_z;
    int64_t valid_blocks;

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
    block_t side_block;
    block_t edge_block;
    int side_level;

    char texname[65];
    char motd[65];

    struct block_defn blockdef[BLOCKMAX];
    int invt_order[BLOCKMAX];

    unsigned char block_perms[BLOCKMAX];

    int version_no;
};
typedef struct block_queue_t block_queue_t;
struct block_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    int curr_offset;
    int queue_len;

    xyzb_t updates[1];
};
typedef struct chat_queue_t chat_queue_t;
struct chat_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    int curr_offset;
    int queue_len;

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
#define SHMID_CHAT	3
#define fcntl_chat_fd shdat.dat[SHMID_CHAT].lock_fd
#define level_chat_queue_len shdat.dat[SHMID_CHAT].len
#define level_chat_queue shdat.chat
#define SHMID_CLIENTS	4
#define client_fd shdat.dat[SHMID_CLIENTS].lock_fd
#define client_list_len shdat.dat[SHMID_CLIENTS].len
#define client_list shdat.client
#define SHMID_BLOCKQ	2
#define level_block_queue_len shdat.dat[SHMID_BLOCKQ].len
#define level_block_queue shdat.blockq
#define SHMID_BLOCKS	1
#define level_blocks_len shdat.dat[SHMID_BLOCKS].len
#define level_blocks shdat.blocks
#define SHMID_PROP	0
#define fcntl_fd shdat.dat[SHMID_PROP].lock_fd
#define level_prop_len shdat.dat[SHMID_PROP].len
#define level_prop shdat.prop
#define INTERFACE 0
