/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
typedef struct help_text_t help_text_t;
struct help_text_t {
    char * item;
    int class;
    char ** lines;
};
extern help_text_t helptext[];
void post_chat(int where,char *chat,int chat_len);
#define UTFNIL	(-257)
#if defined(UTFNIL)
void convert_to_cp437(char *buf,int *l);
#endif
void cmd_help(char *prefix,char *cmdargs);
#define CMD_HELP \
    {N"help", &cmd_help}, {N"faq", &cmd_help}, \
    {N"news", &cmd_help}, {N"view", &cmd_help}, {N"rules", &cmd_help}
#define H_CMD	1	// Help for a command
#define INTERFACE 0
