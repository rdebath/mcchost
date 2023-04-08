
#include "proctitle.h"

#if INTERFACE
#ifdef __FreeBSD__
#define proctitle_init(argc, argv) /*NIL*/
#define proctitle(...) setproctitle(__VA_ARGS__)
#else
#define proctitle_init(argc, argv) set_process_title_init(argc, argv)
#define proctitle(...) set_process_title(__VA_ARGS__)
#endif

#define set_process_title_w __attribute__ ((format (printf, 1, 2)))
#endif

static char * proc_args_mem = 0;
static int    proc_args_len = 0;

void
set_process_title_init(int argc, char ** argv)
{
    proc_args_mem = argv[0];
    proc_args_len = argv[argc-1] + strlen(argv[argc-1]) - argv[0] + 1;
}

void set_process_title_w
set_process_title(char * fmt, ...)
{
    memset(proc_args_mem, 0, proc_args_len);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(proc_args_mem, proc_args_len, fmt, ap);
    va_end(ap);
}
