/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
extern int cp437rom[256];
void print_logfile(char *s);
void cp437_prt(FILE *ofd,int ch);
void write_logfile(char *str,int len);
void close_logfile();
#define LOCAL static
LOCAL void check_reopen_logfile(struct tm *tm);
LOCAL void reopen_logfile();
void open_logfile();
void set_logfile(char *logfile_name);
