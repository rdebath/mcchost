/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
extern int ofd;
void send_discon_msg_pkt(int ofd,char *message);
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
#if !defined(_REENTRANT)
#define USE_FCNTL
#endif
#if !defined(USE_FCNTL)
#include <semaphore.h>
#endif
typedef struct map_info_t map_info_t;
#include <stdint.h>
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
extern volatile map_info_t *level_prop;
void send_spawn_pkt(int ofd,int player_id,char *playername,xyzhv_t posn);
void send_map_file(int ofd);
void start_shared(char *levelname);
void send_server_id_pkt(int ofd,char *servername,char *servermotd);
extern char *level_name;
void cpy_nbstring(char *buf,char *str);
void cpy_nbstring(char *buf,char *str);
void fatal(char *emsg);
void fatal(char *emsg);
void login(void);
void login();
extern int cpe_enabled;
#define MB_STRLEN 64
#define NB_SLEN	(MB_STRLEN+1)
extern char server_salt[NB_SLEN];
extern char server_motd[NB_SLEN];
extern char server_name[NB_SLEN];
extern int cpe_requested;
extern int user_authenticated;
extern char user_id[NB_SLEN];
extern int insize;
extern char inbuf[4096];
extern int ifd;
#define INTERFACE 0
