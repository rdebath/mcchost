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
void send_map_file(int ofd);
#if !defined(_REENTRANT)
#define USE_FCNTL
#endif
#if !defined(USE_FCNTL)
#include <semaphore.h>
#endif
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
