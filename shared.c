#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

/*
 * Shared area control.
 *
 * BEWARE: Need semaphore that clears if the process holding the lock dies.
 *
 *       fcntl() does this, is very simple, but is likely slow.
 *
 *       pthread_mutex_lock() appears to do this.
 *		pthread_mutexattr_setrobust to use "robust futexes"
 *
 *       SysV semops with SEM_UNDO do this.
 *
 *       POSIX Semephores do NOT release on process termination. (sem_wait(1))
 *
 */

#include "shared.h"

// Is -pthreads option provided.
#ifndef _REENTRANT
// ...
#endif

#if INTERFACE
#define SHMID_PROP	0
#define SHMID_BLOCKS	1
#define SHMID_BLOCKQ	2
#define SHMID_CHAT	3
#define SHMID_CLIENTS	4
#define SHMID_COUNT	5

typedef struct shmem_t shmem_t;
struct shmem_t {
    void * ptr;
    intptr_t len;
    int lock_fd;
};

typedef struct shared_data_t shared_data_t;
struct shared_data_t {
    volatile map_info_t *prop;
    volatile block_t *blocks;
    volatile block_queue_t* blockq;
    volatile chat_queue_t *chat;
    volatile client_data_t *client;

    shmem_t dat[SHMID_COUNT];
};

#define level_prop shdat.prop
#define level_prop_len shdat.dat[SHMID_PROP].len
#define fcntl_fd shdat.dat[SHMID_PROP].lock_fd

#define level_blocks shdat.blocks
#define level_blocks_len shdat.dat[SHMID_BLOCKS].len

#define level_block_queue shdat.blockq
#define level_block_queue_len shdat.dat[SHMID_BLOCKQ].len

#define client_list shdat.client
#define client_list_len shdat.dat[SHMID_CLIENTS].len
#define client_fd shdat.dat[SHMID_CLIENTS].lock_fd

#define level_chat_queue shdat.chat
#define level_chat_queue_len shdat.dat[SHMID_CHAT].len
#define fcntl_chat_fd shdat.dat[SHMID_CHAT].lock_fd
#endif

struct shared_data_t shdat;

/*****************************************************************************
 * Shared resources.
 */

/*
    Items:
	level_queue -- recent block changes broadcast

	chat_queue -- chat messages broadcast

	user_chat_queue -- chat messages for one user
	level_chat_queue -- chat messages for one level broadcast
	team_chat_queue -- chat messages for one team broadcast
	    --> Are these all in same queue, just filtered?

	server_prop -- properties of server, not linked to user or level.
	    --> Is this a text config file?

	user_prop -- User properties
	    --> Text file? Blob in data table?
 */

void
open_level_files(char * levelname, int direct)
{
    char sharename[256];
    if (level_prop && level_blocks) return;

    stop_shared();
    stop_block_queue();

    if (strchr(levelname, '/') != 0 || strlen(levelname) > 128) {
	char buf[256];
	snprintf(buf, sizeof(buf), "Illegal level name \"%.40s\"", levelname);
	fatal(buf);
        return;
    }

    snprintf(sharename, sizeof(sharename), LEVEL_PROPS_NAME, levelname);
    allocate_shared(sharename, sizeof(*level_prop), shdat.dat+SHMID_PROP);
    level_prop = shdat.dat[SHMID_PROP].ptr;

    lock_shared();

    if (!direct) {
	if (level_prop->version_no != MAP_VERSION || level_prop->magic_no != MAP_MAGIC
	    || level_prop->cells_x == 0 || level_prop->cells_y == 0 || level_prop->cells_z == 0)
	{

	    int ok = 0;
	    // If level missing -- extract the matching *.cw file
	    char cwfilename[256];
	    snprintf(cwfilename, sizeof(cwfilename), LEVEL_CW_NAME, levelname);
	    if (access(cwfilename, R_OK) == 0) {
		ok = (load_map_from_file(cwfilename, levelname) >= 0);
	    }

	    // If level missing -- extract a model *.cw file
	    if (!ok && access(MODEL_CW_NAME, R_OK) == 0) {
		ok = (load_map_from_file(MODEL_CW_NAME, levelname) >= 0);
	    }

	    if (!ok) level_prop->version_no = 0;
	}
    }

    // If level missing -- make a flat.
    if (level_prop->version_no != MAP_VERSION || level_prop->magic_no != MAP_MAGIC
	|| level_prop->cells_x == 0 || level_prop->cells_y == 0 || level_prop->cells_z == 0)
	createmap(levelname);
    else
        // NB: Missing file here makes an Air map.
        open_blocks(levelname);

    unlock_shared();
}

void
open_blocks(char * levelname)
{
    char sharename[256];
    if (level_blocks) {
	intptr_t sz = level_blocks_len;
	(void)munmap((void*)level_blocks, sz);
	level_blocks_len = 0;
	level_blocks = 0;
    }

    sprintf(sharename, LEVEL_BLOCKS_NAME, levelname);
    level_blocks_len = (uintptr_t)level_prop->cells_x * level_prop->cells_y *
	level_prop->cells_z * sizeof(*level_blocks) +
	sizeof(map_len_t);
    allocate_shared(sharename, level_blocks_len, shdat.dat+SHMID_BLOCKS);
    level_blocks = shdat.dat[SHMID_BLOCKS].ptr;

    close(shdat.dat[SHMID_BLOCKS].lock_fd);
    shdat.dat[SHMID_BLOCKS].lock_fd = 0;
}

void
stop_shared(void)
{
    if (level_prop) {
	intptr_t sz = level_prop_len;
	(void)munmap((void*)level_prop, sz);
	level_prop_len = 0;
	level_prop = 0;
    }

    if (level_blocks) {
	intptr_t sz = level_blocks_len;
	(void)munmap((void*)level_blocks, sz);
	level_blocks_len = 0;
	level_blocks = 0;
    }

    if (level_block_queue)
	stop_block_queue();

    if (fcntl_fd > 0) {
	close(fcntl_fd);
	fcntl_fd = 0;
    }
}

void
create_block_queue(char * levelname)
{
    char sharename[256];
    int queue_count = (level_prop->queue_len + 1024) / 1024 * 1024;
    if (queue_count < 2048) queue_count = 2048;

    if (level_prop->last_map_download_size / msglen[PKID_SRVBLOCK] > queue_count) {
	queue_count = level_prop->last_map_download_size / msglen[PKID_SRVBLOCK];
	queue_count = (queue_count + 1024) / 1024 * 1024;
	level_prop->queue_len = queue_count;
    }

    stop_block_queue();
    wipe_last_block_queue_id();

    sprintf(sharename, LEVEL_QUEUE_NAME, levelname);
    level_block_queue_len = sizeof(*level_block_queue) + queue_count * sizeof(xyzb_t);
    allocate_shared(sharename, level_block_queue_len, shdat.dat+SHMID_BLOCKQ);
    level_block_queue = shdat.dat[SHMID_BLOCKQ].ptr;

    lock_shared();
    if (level_block_queue->generation == 0 ||
	level_block_queue->curr_offset >= level_block_queue->queue_len ||
	level_block_queue->queue_len != queue_count) {

	if (level_block_queue->curr_offset >= level_block_queue->queue_len)
	    level_block_queue->queue_len = 0;

	if (queue_count > level_block_queue->queue_len ||
		level_block_queue->queue_len > level_prop->total_blocks) {
	    level_block_queue->generation += 2;
	    level_block_queue->curr_offset = 0;
	    level_block_queue->queue_len = queue_count;
	}
	else queue_count = level_block_queue->queue_len;

	unlock_shared();

	stop_block_queue();
	level_block_queue_len = sizeof(*level_block_queue) + queue_count * sizeof(xyzb_t);
	allocate_shared(sharename, level_block_queue_len, shdat.dat+SHMID_BLOCKQ);
	level_block_queue = shdat.dat[SHMID_BLOCKQ].ptr;

	close(shdat.dat[SHMID_BLOCKQ].lock_fd);
	shdat.dat[SHMID_BLOCKQ].lock_fd = 0;

    } else
	unlock_shared();
}

void
stop_block_queue()
{
    if (level_block_queue) {
	intptr_t sz = level_block_queue_len;
	(void)munmap((void*)level_block_queue, sz);
	level_block_queue_len = 0;
	level_block_queue = 0;
	wipe_last_block_queue_id();
    }
}

void
open_client_list()
{
    char sharename[256];

    if (client_list) stop_client_list();

    sprintf(sharename, SYS_USER_LIST_NAME);
    client_list_len = sizeof(*client_list);
    allocate_shared(sharename, client_list_len, shdat.dat+SHMID_CLIENTS);
    client_list = shdat.dat[SHMID_CLIENTS].ptr;
    if (!client_list) return;

    if (client_list->magic1 != MAGIC_USR || client_list->magic2 != MAGIC_USR) {
	client_data_t d = { .magic1 = MAGIC_USR, .magic2 = MAGIC_USR };
	*client_list = d;
    }
}

void
stop_client_list()
{
    if (client_list) {
	intptr_t sz = client_list_len;
	(void)munmap((void*)client_list, sz);
	client_list_len = 0;
	client_list = 0;
    }

    if (client_fd > 0) {
	close(client_fd);
	client_fd = 0;
    }
}

void
create_chat_queue()
{
    char sharename[256];
    int queue_count = 0;
    if (queue_count < 128) queue_count = 128;
    if (queue_count > 65536) queue_count = 65536;

    stop_chat_queue();
    wipe_last_chat_queue_id();

    sprintf(sharename, CHAT_QUEUE_NAME);
    level_chat_queue_len = sizeof(*level_chat_queue) + queue_count * sizeof(chat_entry_t);
    allocate_shared(sharename, level_chat_queue_len, shdat.dat+SHMID_CHAT);
    level_chat_queue = shdat.dat[SHMID_CHAT].ptr;

    lock_chat_shared();
    if (    level_chat_queue->generation == 0 ||
	    level_chat_queue->queue_len != queue_count ||
	    level_chat_queue->curr_offset >= queue_count) {
	level_chat_queue->generation += 2;
	level_chat_queue->curr_offset = 0;
	level_chat_queue->queue_len = queue_count;
	unlock_chat_shared();
    } else
	unlock_chat_shared();

    set_last_chat_queue_id();
}

void
stop_chat_queue()
{
    if (level_chat_queue) {
	intptr_t sz = level_chat_queue_len;
	(void)munmap((void*)level_chat_queue, sz);
	level_chat_queue_len = 0;
	level_chat_queue = 0;
	wipe_last_chat_queue_id();
    }

    if (fcntl_chat_fd > 0) {
	close(fcntl_chat_fd);
	fcntl_chat_fd = 0;
    }
}

LOCAL void
allocate_shared(char * share_name, int share_size, shmem_t *shm)
{
    intptr_t sz;
    void * shm_p;
    int shared_fd = -1;

    shared_fd = open(share_name, O_CREAT|O_RDWR|O_NOFOLLOW, 0600);
    if (shared_fd < 0) {
	char buf[256];
	sprintf(buf, "Cannot open mapfile \"%.40s\"", share_name);
	fatal(buf);
        return;
    }

    {
	int p = sysconf(_SC_PAGESIZE);
	sz = share_size;
	sz += p - 1;
	sz -= sz % p;
    }

    if (posix_fallocate(shared_fd, 0, sz) < 0) {
	perror("posix_fallocate");
	fatal("fallocate cannot allocate disk space");
        return;
    }

    shm_p = (void*) mmap(0,
		    sz,
		    PROT_READ|PROT_WRITE,
		    MAP_SHARED,
		    shared_fd, 0);

    if ((intptr_t)shm_p == -1) {
	perror("mmap");
	fatal("Cannot allocate shared area");
        return;
    }

    shm->ptr = shm_p;
    shm->len = sz;
    shm->lock_fd = shared_fd;
}

void
lock_shared(void)
{
   share_lock(fcntl_fd, F_SETLKW, F_WRLCK);
}

void
unlock_shared(void)
{
   share_lock(fcntl_fd, F_SETLK, F_UNLCK);
}

void
lock_chat_shared(void)
{
   share_lock(fcntl_chat_fd, F_SETLKW, F_WRLCK);
}

void
unlock_chat_shared(void)
{
   share_lock(fcntl_chat_fd, F_SETLK, F_UNLCK);
}

LOCAL void
share_lock(int fd, int mode, int l_type)
{
   struct flock lck;
   int rv;

   for(;;)
   {
      lck.l_type = l_type;
      lck.l_whence = SEEK_SET;
      lck.l_start = 0;
      lck.l_len = 0;

      rv = fcntl(fd, mode, &lck);
      if (rv >= 0) return;
      if (rv < 0 && errno != EINTR) {
	 perror("Problem locking shared area");
	 return;
      }
   }
}

