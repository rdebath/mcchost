
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "filenames.h"

#if INTERFACE
#define SERVER_CONF_NAME "server.ini"
#define LEVEL_PROPS_NAME "level/%s.props"
#define LEVEL_BLOCKS_NAME "level/%s.blocks"
#define LEVEL_QUEUE_NAME "level/%s.queue"
#define LEVEL_MAP_DIR_NAME "map"
#define LEVEL_CW_NAME LEVEL_MAP_DIR_NAME "/%s.cw"
#define LEVEL_TMP_NAME LEVEL_MAP_DIR_NAME "/%s.tmp"
#define MODEL_CW_NAME "model.cw"
#define SYS_STAT_NAME "system/userlevels.dat"
#define CHAT_QUEUE_NAME "system/chat.queue"
#define LEVEL_BACKUP_DIR_NAME "backup"
#define LEVEL_PREV_NAME LEVEL_BACKUP_DIR_NAME "/%s.cw"
#define LEVEL_BACKUP_NAME LEVEL_BACKUP_DIR_NAME "/%s.%d.cw"
#endif

static char * dirlist[] = {
    "system",
    "level",
    LEVEL_MAP_DIR_NAME,
    LEVEL_BACKUP_DIR_NAME,
    "help",
    "log",
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

static char hex[] = "0123456789ABCDEF";

/*
 * Given a CP437 levelname create the ASCII version.
 */
void
fix_fname(char *buf, int len, char *s)
{
    char *d = buf;
    if (len<=0) return;
    for(char *p=s;*p;p++) {
	if (d>=buf+len-1) break;
	if (*p > ' ' && *p < '~' && *p != '/' && *p != '\\'
		&& *p != '%' && *p != '.') {
	    *d++ = *p;
	} else {
	    *d++ = '%';
	    *d++ = hex[(*p>>4)&0xF];
	    *d++ = hex[*p&0xF];
	}
    }
    *d = 0;
}

/*
 * Given a valid ASCII levelname create the CP437 version.
 * Case sensitive!
 */
void
unfix_fname(char *buf, int len, char *s)
{
    char *d = buf;
    if (len<=0) return;
    if (*s == 0 || *s == '.') { *buf = 0; return; }

    for(char *p=s;*p;p++) {
	if (d>=buf+len-1) { *buf=0; return; }
	if (*p <= ' ' || *p >= '~' || *p == '/' || *p == '\\' || *p == '.') {
	    *buf = 0; return;
	}
	if (*p == '%') {
	    char *d1, *d2;
	    if (p[1] == 0 || (d1 = strchr(hex, p[1])) == 0 ||
	        p[2] == 0 || (d2 = strchr(hex, p[2])) == 0) {
		*buf = 0; return;
	    }
	    p+=2;
	    int ch = (d1-hex)*16 + (d2-hex);
	    if (ch == 0 || (ch > ' ' && ch < '~' && ch != '/' && ch != '\\'
		&& ch != '_' && ch != '%' && ch != '.')) {
		*buf = 0; return;
	    }
	    *d++ = ch;
	} else
	    *d++ = *p;
    }
    *d = 0;
}
