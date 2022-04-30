
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
#define MODEL_CW_NAME "model.cw"
#define SYS_USER_LIST_NAME "system/user.list"
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
