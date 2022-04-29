/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
extern int cp437rom[256];
int decodeutf8(int ch,int *utfstate);
#define UTFNIL	(-257)
void post_chat(int where,char *chat,int chat_len);
#if defined(UTFNIL)
void convert_to_cp437(char *buf,int *l);
#endif
void cmd_help(char *cmdargs);
