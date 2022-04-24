/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void flush_to_remote();
void send_discon_msg_pkt(char *message);
void logout(char *emsg);
void kicked(char *emsg);
void disconnect(char *emsg);
typedef struct shared_data_t shared_data_t;
typedef struct map_info_t map_info_t;
#include <stdint.h>
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
typedef struct xyzb_t xyzb_t;
struct xyzb_t { uint16_t x, y, z, b; };
struct block_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    int curr_offset;
    int queue_len;

    xyzb_t updates[1];
};
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
void print_logfile(char *s);
typedef struct MD5_CTX MD5_CTX;
typedef unsigned int UINT4;
struct MD5_CTX {
  UINT4 i[2];                   /* number of _bits_ handled mod 2^64 */
  UINT4 buf[4];                                    /* scratch buffer */
  unsigned char in[64];                              /* input buffer */
  unsigned char digest[16];     /* actual digest after MD5Final call */
};
void MD5Final(MD5_CTX *mdContext);
void MD5Update(MD5_CTX *mdContext,unsigned char *inBuf,unsigned int inLen);
void MD5Init(MD5_CTX *mdContext);
void stop_user();
void run_request_loop();
void post_chat(char *chat,int chat_len);
void send_message_pkt(int id,char *message);
#define level_prop shdat.prop
void send_spawn_pkt(int player_id,char *playername,xyzhv_t posn);
void send_map_file();
void open_level_files(char *levelname);
void create_chat_queue();
void start_user();
void send_server_id_pkt(char *servername,char *servermotd,int user_type);
void open_client_list();
void process_connection();
void tcpserver();
void process_args(int argc,char **argv);
extern int proc_args_len;
extern char *proc_args_mem;
extern char *level_name;
void cpy_nbstring(char *buf,char *str);
void cpy_nbstring(char *buf,char *str);
void fatal(char *emsg);
void fatal(char *emsg);
void login(void);
void login();
extern int enable_cp437;
extern block_t max_blockno_to_send;
extern int cpe_enabled;
extern char server_salt[NB_SLEN];
extern char server_motd[NB_SLEN];
extern char server_name[NB_SLEN];
extern int tcp_port_no;
extern int start_tcp_server;
extern int cpe_requested;
extern int server_id_op_flag;
extern int user_authenticated;
extern char user_id[NB_SLEN];
extern int insize;
extern char inbuf[4096];
extern int line_ifd;
extern int line_ofd;
#define INTERFACE 0
