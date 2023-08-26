
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "filenames.h"

#if INTERFACE
#define SERVER_CONF_NAME "server.ini"
#define SERVER_CONF_TMP "server.%d.tmp"
#define SERVER_CONF_PORT "server.%d.ini"
#define LEVEL_PROPS_NAME "level/%s.props"
#define LEVEL_BLOCKS_NAME "level/%s.blocks"
#define LEVEL_QUEUE_NAME "level/%s.queue"
#define LEVEL_TMPBLOCKS_NAME "level/%s.tmpblk"

#define LEVEL_MAP_DIR_NAME "map"
#define LEVEL_CW_NAME LEVEL_MAP_DIR_NAME "/%s.cw"
#define LEVEL_TMP_NAME LEVEL_MAP_DIR_NAME "/%s.tmp"
#define LEVEL_BAK_NAME LEVEL_MAP_DIR_NAME "/%s.bak"
#define LEVEL_INI_NAME LEVEL_MAP_DIR_NAME "/%s.ini"
#define LEVEL_TMPINI_NAME LEVEL_MAP_DIR_NAME "/%s.ini-"

#define LEVEL_BACKUP_DIR_NAME "backup"
#define LEVEL_BACKUP_NAME LEVEL_BACKUP_DIR_NAME "/%s.%d.cw"
#define LEVEL_BACKUP_NAME_1 LEVEL_BACKUP_DIR_NAME "/%s.cw"

#define USER_DIR "user"
#define USER_INI_NAME USER_DIR "/%s.ini"

#define SECRET_DIR "secret"
#define SECRET_PW_NAME "secret/%s.pwl"

#define MODEL_INI_NAME "model.ini"
#define SYS_CONF_NAME "system/system.dat"
#define SYS_STAT_NAME "system/userlevels.dat"
#define CHAT_QUEUE_NAME "system/chat.queue"
#define CMD_QUEUE_NAME "system/cmd.queue"

#define SYS_LOCK_NAME "system/system.lock"
#define CHAT_LOCK_NAME "system/chat.lock"
#define CMD_LOCK_NAME "system/cmd.lock"
#define SAVE_LOCK_NAME "system/saving.lock"
#define LEVEL_LOCK_NAME "level/%s.lock"

#define CMDSET_CONF_NAME "cmdset.ini"

#define LOGFILE_SYMLINK_NAME "log/current.log"
#endif

char * Version = VERSION;

char * game_user = 0;
char * game_group = 0;

static char * dirlist[] = {
    "system",
    "level",
    LEVEL_MAP_DIR_NAME,
    LEVEL_BACKUP_DIR_NAME,
    USER_DIR,
    SECRET_DIR,
    "texture",
    "help",
    "log",
    "ini",
    0
};

#define E(_x) ERR((_x), #_x)
static inline void ERR(int n, char * tn)
{
    if (n == -1) { perror(tn); exit(9); }
}

void
find_dirs() {
    // The current directory has not been explicity set as the one we
    // should work in so move to the default. (Use symlink to move that)

    // Special case: If we've been started as root try to use the login
    // id "games" and store our maps in /var/games/mcchost.

    const char *home_d = 0;
    int vargames = 0;
    uid_t id = getuid();
    uid_t run_as = 0;
    gid_t grun_as = 0;

    if (id > 0)
	if ((home_d = getenv("HOME")) == 0)
	    home_d = getpwuid(id)->pw_dir;

    if (!home_d) vargames = 1;

    if (id == 0) {
	vargames = 1;
	fetch_ids(&run_as, &grun_as);
	if (game_user)
            home_d = getpwuid(run_as)->pw_dir;
	if (home_d) vargames = 0;
    }

    char dirpath[PATH_MAX];
    if (vargames)
	strcpy(dirpath, "/var/games/mcchost"); // Just some random location.
    else
	saprintf(dirpath, "%s/.mcchost", home_d);

    struct stat st;
    if (stat(dirpath, &st) >= 0 && (st.st_mode & S_IFMT) != 0) {
	// Game directory exists --> use it.
    } else {
	// Make /var/games if it doesn't exist.
	if (id == 0 && vargames) (void) mkdir("/var/games", 0755);

	// Try to create the directory.
	ERR(mkdir(dirpath, 0700), dirpath);
	if (id == 0 && vargames) {
	    if (grun_as) E(chmod(dirpath, 02770));
	    if (run_as || grun_as)
		E(chown(dirpath, run_as, grun_as));
	}
    }

    if (id == 0) {
	if (grun_as) E(setgid(grun_as));
	if (run_as) E(setuid(run_as));
	if (vargames && grun_as) (void) umask(007);
    }

    ERR(chdir(dirpath), dirpath);
}

void
init_dirs()
{
    if (getuid() == 0) {
        uid_t run_as = 0;
	gid_t grun_as = 0;
	fetch_ids(&run_as, &grun_as);
	E(setgid(grun_as));
	E(setuid(run_as));
    }

    for(int i = 0; dirlist[i]; i++) {
	if (mkdir(dirlist[i], 0777) < 0 && errno != EEXIST) {
	    char buf[256];
	    saprintf(buf, "Failure creating directory: \"%s\"", dirlist[i]);
	    perror(buf);
	    // System is rather essential, but ...
	    if (!i) exit(127);
	    // Just complain as later processes will error.
	}
    }

    FILE * fd = fopen("Readme.txt", "w");
    if (fd) {
	fprintf(fd, "%s version: %s\n%s", SWNAME, Version, directory_readme);
	fclose(fd);
    }
}

LOCAL void
fetch_ids(uid_t *p_uid, gid_t *p_gid)
{
    struct passwd *uid = getpwnam(game_user);
    if (uid) *p_uid = uid->pw_uid;
    struct group * gid = getgrnam(game_group);
    if (gid) *p_gid = gid->gr_gid;

    if (!uid || !gid) {
	fprintf(stderr, "Unable to switch to user and group \"%s\" and \"%s\"\n", game_user, game_group);
	exit(9);
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
    if (s) for(char *p=s;*p;p++) {
	if (d>=buf+len-1) break;
	if (*p >= ' ' && *p <= '~' && *p != '/' && *p != '\\' && *p != '.') {
	    *d++ = *p;
	} else {
	    int uc = cp437rom[(*p) & 0xFF];
	    int c1, c2, c3;

	    if (uc == '/')  uc = 0x2215; // DIVISION SLASH
	    if (uc == '\\') uc = 0x2216; // SET MINUS
	    if (uc == '.') {
		uc = 0x2024; // ONE DOT LEADER
		if (p[1] == '.') {
		    p++;
		    uc = 0x2025; // TWO DOT LEADER
		    if (p[1] == '.') {
			p++;
			uc = 0x2026; // HORIZONTAL ELLIPSIS
		    }
		}
	    }
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
	int ch = -1, rep = 0;
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
		else if (utf == 0x2024) ch = '.';
		else if (utf == 0x2025) {ch = '.'; rep=1;}
		else if (utf == 0x2026) {ch = '.'; rep=2;}
		else
		for(int n=0; n<256; n++) {
		    if (cp437rom[(n+128)&0xFF] == utf)
			{ ch=((n+128)&0xFF) | 0x100; break; }
		}
	    }
	}

	if (ch <= 0) { *buf = 0; return; }
	*d++ = ch;
	for( ;rep>0; rep--) {
	    if (d>=buf+len-1) { *buf=0; return; }
	    *d++ = ch;
	}
    }
    *d = 0;
}

#if INTERFACE
// lmdb has terrible compatibility
#if defined(__LP64__) && defined(__x86_64__)
#define USERDB_FILE "system/userdb64.mdb"
#define USERDB_RECREATE	0
#endif

#if defined(__ILP32__) && defined(__x86_64__)
#define USERDB_FILE "system/userdbx32.mdb"
#define USERDB_RECREATE	0
#endif

#if defined(__i386__)
#define USERDB_FILE "system/userdb32.mdb"
#define USERDB_RECREATE	0
#endif

#if !defined(__x86_64__) && !defined(__i386__)
#define USERDB_FILE "system/userdb.mdb"
#define USERDB_RECREATE	1
#endif

#endif

char directory_readme[] =
"\n"	"Contents of working directory."
"\n"
"\n"	"help:"
"\n"	"    Contains files displayed by the \"/help\",\"/faq\" and \"/news\" commands."
"\n"
"\n"	"    News is stored in the \"news.txt\" file and any file matching"
"\n"	"    \"news_*.txt\".  Similarly for the \"/faq\" command.  The \"/help\" command"
"\n"	"    picks up \"help.txt\" by default and any argments given show a file"
"\n"	"    matching that name. Filenames are always in lowercase and spaces"
"\n"	"    (and other problematic characters) are replaced by \"_\". The file"
"\n"	"    that any particular help command fails to find will be logged."
"\n"
"\n"	"map:"
"\n"	"    This is where currently saved versions of all maps are stored."
"\n"	"    Note: level names are remapped to UTF-8 file names so any in game"
"\n"	"    character can be used in a filename, but some of the translations"
"\n"	"    may not be as expected for safety reasons."
"\n"
"\n"	"level:"
"\n"	"    Contains files for currently \"loaded\" or \"unpacked\" levels. Data in"
"\n"	"    here is the most current version of the level before any official"
"\n"	"    \"save\" function. This directory must be on a fast local filesystem."
"\n"	"    This directory is usually saved on clean shutdown, however, any"
"\n"	"    unpacked levels will be considered newer than the map directory."
"\n"
"\n"	"backup:"
"\n"	"    Level backups are stored here. This directory can be safely moved"
"\n"	"    by replacing it with a symbolic link which can point to local remote"
"\n"	"    or various virtual filesystem. If access to the filesystem fails it"
"\n"	"    will be retried."
"\n"
"\n"	"user:"
"\n"	"    The user configuration data (except passwords) is stored here."
"\n"
"\n"	"secret:"
"\n"	"    User passwords for the \"/pass\" and \"/setpass\" commands are stored"
"\n"	"    here; passwords are hashed, but keep then safe anyway."
"\n"
"\n"	"system:"
"\n"	"    Data for internal communications and indexing.  If the server is"
"\n"	"    shutdown this directory contains no unique data and may be deleted."
"\n"	"    This directory must be on a fast local filesystem."
"\n"
"\n"	"ini:"
"\n"	"    Files for the \"/inisave\" and \"/iniload\" commands. These command can"
"\n"	"    be used to export/import map settings."
"\n"
"\n"	"texture:"
"\n"	"    This directory contains files served by the built in silly little"
"\n"	"    web server."
"\n"
"\n"	"server.ini and server.25565.ini:"
"\n"	"    Server wide settings."
"\n"
"\n"	"cmdset.ini:"
"\n"	"    Define primary security level for all commands, including disabled."

;
