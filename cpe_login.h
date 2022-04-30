/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void complete_connection();
extern int cpe_pending;
extern int cpe_extn_remaining;
typedef struct pkt_extentry pkt_extentry;
#define MB_STRLEN 64
#define NB_SLEN (MB_STRLEN+1)
struct pkt_extentry {
    char extname[NB_SLEN];
    int version;
};
void process_extentry(pkt_extentry *pkt);
void send_customblocks_pkt();
void send_extentry_pkt(char *extension,int version);
extern char server_software[NB_SLEN];
void send_extinfo_pkt(char *appname,int num_extn);
void send_ext_list();
extern int extn_setspawnpoint;
extern int extn_evilbastard;
extern int extn_sethotbar;
extern int extn_instantmotd;
extern int extn_longermessages;
extern int extn_fullcp437;
extern int extn_clickdistance;
extern int extn_customblocks;
typedef struct ext_list_t ext_list_t;
struct ext_list_t {
    char * name;
    int version;
    int *enabled_flag;
    int enabled;
};
#define INTERFACE 0
