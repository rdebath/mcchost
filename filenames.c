
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "filenames.h"

#if INTERFACE
#define SERVER_CONF_NAME "server.ini"
#define LEVEL_PROPS_NAME "level/%s.props"
#define LEVEL_BLOCKS_NAME "level/%s.blocks"
#define LEVEL_QUEUE_NAME "level/%s.queue"
#define LEVEL_CW_NAME "map/%s.cw"
#define LEVEL_TMP_NAME "map/%s.tmp"
#define MODEL_CW_NAME "model.cw"
#define SYS_STAT_NAME "system/userlevels.dat"
#define CHAT_QUEUE_NAME "system/chat.queue"
#endif

static char * dirlist[] = {
    "system",
    "level",
    "help",
    "backup",
    "log",
    "map",
    0
};

void
init_dirs()
{
    for(int i = 0; dirlist[i]; i++) {
	if (mkdir(dirlist[i], 0777) < 0 && errno != EEXIST) {
	    perror(dirlist[i]);
	    // Just complain, later processes will error.
	}
    }
}

void
fix_fname(char *buf, int len, char *s)
{
    char *d = buf;
    if (len<0) return;
    for(char *p=s;*p;p++) {
	if (d>=buf+len-1) break;
	if (*p > ' ' && *p < '~' && *p != '\'' && *p != '/' && *p != '\\'
		&& *p != '_' && *p != '%' && !(*p == '.' && d == buf)) {
	    *d++ = *p;
	} else if (*p == ' ') {
	    *d++ = '_';
	} else {
	    static char hex[] = "0123456789ABCDEF";
	    *d++ = '%';
	    *d++ = hex[(*p>>4)&0xF];
	    *d++ = hex[*p&0xF];
	}
    }
    *d = 0;
}
