/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
extern char cp437_ascii[];
int decodeutf8(int ch,int *utfstate);
#define UTFNIL	(-257)
#if defined(UTFNIL)
void convert_to_cp437(char *buf,int *l);
#endif
extern int cp437rom[256];
void cp437_prt(FILE *ofd,int ch);
