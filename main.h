/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#define MB_STRLEN 64
void flush_to_remote();
void send_discon_msg_pkt(char *message);
#define LOCAL static
void logout(char *emsg);
LOCAL void disconnect(int rv,char *emsg);
typedef struct shared_data_t shared_data_t;
typedef struct map_info_t map_info_t;
#include <stdint.h>
typedef struct xyzhv_t xyzhv_t;
struct xyzhv_t { int x, y, z; int8_t h, v, valid; };
typedef uint16_t block_t;
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
extern int client_ipv4_localhost;
extern char client_ipv4_str[INET_ADDRSTRLEN];
extern int client_ipv4_port;
void cpy_nbstring(char *buf,char *str);
void quiet_drop(char *emsg);
void print_logfile(char *s);
void cmd_help(char *prefix,char *cmdargs);
void post_chat(int where,char *chat,int chat_len);
void send_message_pkt(int id,char *message);
void send_clickdistance_pkt(int dist);
extern int extn_clickdistance;
void send_welcome_message();
#define level_prop shdat.prop
void send_spawn_pkt(int player_id,char *playername,xyzhv_t posn);
void send_map_file();
void open_level_files(char *levelname,int direct);
void create_chat_queue();
void start_user();
void send_server_id_pkt(char *servername,char *servermotd,int user_type);
void fatal(char *emsg);
extern int extn_evilbastard;
void complete_connection();
void send_ext_list();
void login();
void open_client_list();
void stop_user();
void send_disconnect_message();
void run_request_loop();
void process_connection();
void tcpserver();
void open_logfile();
void init_dirs();
void process_args(int argc,char **argv);
extern int proc_args_len;
extern char *proc_args_mem;
extern char *level_name;
extern int enable_cp437;
extern block_t max_blockno_to_send;
extern int cpe_pending;
extern int cpe_enabled;
extern int server_runonce;
extern int server_private;
extern char heartbeat_url[1024];
extern char client_software[NB_SLEN];
extern char server_secret[NB_SLEN];
extern char server_motd[NB_SLEN];
extern char server_name[NB_SLEN];
extern char server_software[NB_SLEN];
extern char program_name[512];
extern int tcp_port_no;
extern int enable_heartbeat_poll;
extern int start_tcp_server;
extern int cpe_extn_remaining;
extern int cpe_requested;
extern int ignore_cpe;
extern int server_id_op_flag;
extern int user_authenticated;
extern char user_id[NB_SLEN];
extern int insize;
extern char inbuf[4096];
extern int line_ifd;
extern int line_ofd;
