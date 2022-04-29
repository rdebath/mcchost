/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
extern int cp437rom[256];
int decodeutf8(int ch,int *utfstate);
#define UTFNIL	(-257)
typedef struct help_text_t help_text_t;
struct help_text_t {
    char * item;
    int class;
    char ** lines;
};
extern help_text_t helptext[];
#define HAS_HELP
void post_chat(int where,char *chat,int chat_len);
#if defined(UTFNIL)
void convert_to_cp437(char *buf,int *l);
#endif
void cmd_help(char *prefix,char *cmdargs);
#define H_CMD	1	// Help for a command
#define INTERFACE 0
