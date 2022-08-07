
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "filenames.h"

#if INTERFACE
#define SERVER_CONF_NAME "server.ini"
#define SERVER_CONF_TMP "server.%d.tmp"
#define SERVER_CONF_PORT "server.%d.ini"
#define LEVEL_PROPS_NAME "level/%s.props"
#define LEVEL_BLOCKS_NAME "level/%s.blocks"
#define LEVEL_QUEUE_NAME "level/%s.queue"

#define LEVEL_MAP_DIR_NAME "map"
#define LEVEL_CW_NAME LEVEL_MAP_DIR_NAME "/%s.cw"
#define LEVEL_TMP_NAME LEVEL_MAP_DIR_NAME "/%s.tmp"
#define LEVEL_BAK_NAME LEVEL_MAP_DIR_NAME "/%s.bak"

#define LEVEL_BACKUP_DIR_NAME "backup"
#define LEVEL_BACKUP_NAME LEVEL_BACKUP_DIR_NAME "/%s.%d.cw"

#define USERDB_DIR "userdb"

#define MODEL_CW_NAME "model.cw"
#define MODEL_INI_NAME "model.ini"
#define SYS_CONF_NAME "system/system.dat"
#define SYS_STAT_NAME "system/userlevels.dat"
#define CHAT_QUEUE_NAME "system/chat.queue"

#define SYS_LOCK_NAME "system/system.lock"
#define CHAT_LOCK_NAME "system/chat.lock"
#define LEVEL_LOCK_NAME "level/%s.lock"
#endif

static char * dirlist[] = {
    "system",
    "level",
    LEVEL_MAP_DIR_NAME,
    LEVEL_BACKUP_DIR_NAME,
    USERDB_DIR,
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

/*
 * Given a CP437 levelname create the UTF-8 version.
 */
void
fix_fname(char *buf, int len, char *s)
{
    char *d = buf;
    if (len<=0) return;
    for(char *p=s;*p;p++) {
	if (d>=buf+len-1) break;
	if (*p >= ' ' && *p <= '~' && *p != '/' && *p != '\\' && *p != '.') {
	    *d++ = *p;
	} else {
	    int uc = cp437rom[(*p) & 0xFF];
	    int c1, c2, c3;

	    if (uc == '/')  uc = 0x2215; // DIVISION SLASH;
	    if (uc == '\\') uc = 0x2216; // SET MINUS
	    if (uc == '.')  uc = 0x1390; // ETHIOPIC TONAL MARK YIZET
	    if (uc < 0x80)  uc = 0xF000 + (uc&0xFF); // Linux direct to font.

	    c2 = (uc/64);
	    c1 = uc - c2*64;
	    c3 = (c2/64);
	    c2 = c2 - c3 * 64;
	    if (uc < 128 && uc >= 0) {
		*d++ = uc;
	    } else if (uc < 2048) {
		*d++ = c2+192; *d++ = c1+128;
	    } else if (uc < 65536) {
		*d++ = c3+224; *d++ = c2+128; *d++ = c1+128;
	    } else {
		*d++ = 0xef; *d++ = 0xbf; *d++ = 0xbd;
	    }
	}
    }
    *d = 0;
}

/*
 * Given a valid level filename create the CP437 version.
 * Case sensitive!
 */
void
unfix_fname(char *buf, int len, char *s)
{
    char *d = buf;
    int utfstate[1] = {0};
    *buf = 0;
    if (len<2 || *s == 0 || *s == '.') return;

    for(char *p=s;*p;p++) {
	if (d>=buf+len-1) { *buf=0; return; }
	int ch = -1;
	if (*utfstate>=0) ch= (*p & 0xFF); else { p--; ch = -1; }

	if (ch <= 0x7F && *utfstate == 0) {
	    if (*p < ' ' || *p > '~' || *p == '/' || *p == '\\' || *p == '.')
		{ *buf = 0; return; }
	} else {
	    ch = decodeutf8(ch, utfstate);
	    if (ch == UTFNIL)
		continue;
	    if (ch >= 0x80) {
		int utf = ch;
		ch = -1;
		if (utf == 0x2215) ch = '/';
		else if (utf == 0x2216) ch = '\\';
		else if (utf == 0x1390) ch = '.';
		else
		for(int n=0; n<256; n++) {
		    if (cp437rom[(n+128)&0xFF] == utf)
			{ ch=((n+128)&0xFF) | 0x100; break; }
		}
	    }
	}

	if (ch <= 0) { *buf = 0; return; }
	*d++ = ch;
    }
    *d = 0;
}
