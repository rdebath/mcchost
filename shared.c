#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
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
 *		However, it goes into an error state.
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
#include <stdint.h>

#define SHMID_PROP	0
#define SHMID_BLOCKS	1
#define SHMID_BLOCKQ	2
#define SHMID_CHAT	3
#define SHMID_CLIENTS	4
#define SHMID_COUNT	5

typedef struct shmem_t shmem_t;
struct shmem_t {
    void * ptr;
    uintptr_t len;
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
#define level_blocks shdat.blocks
#define level_block_queue shdat.blockq
#define client_list shdat.client
#define level_chat_queue shdat.chat
#endif

#define fcntl_fd shdat.dat[SHMID_PROP].lock_fd
#define client_fd shdat.dat[SHMID_CLIENTS].lock_fd
#define chat_fd shdat.dat[SHMID_CHAT].lock_fd

#define level_block_queue_len shdat.dat[SHMID_BLOCKQ].len
#define level_chat_queue_len shdat.dat[SHMID_CHAT].len

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

/*
 * direct:
 *    0: open or create
 *    1: open or create just the props
 *    2: just open don't create
 */
void
open_level_files(char * levelname, int direct)
{
    char sharename[256];
    int del_on_err = 0, try_cw_file = 0;
    if (level_prop && level_blocks) return;

    stop_shared();

    check_level_name(levelname); // Last check.

    snprintf(sharename, sizeof(sharename), LEVEL_BLOCKS_NAME, levelname);
    del_on_err = access(sharename, F_OK);
    snprintf(sharename, sizeof(sharename), LEVEL_PROPS_NAME, levelname);
    if (del_on_err == 0)
	del_on_err = access(sharename, F_OK);
    if (del_on_err < 0) {
	if(errno == EACCES) del_on_err = 0;
	else del_on_err = 1;
    } else del_on_err = 0;

    if (del_on_err && direct == 2) return;

    if (allocate_shared(sharename, sizeof(*level_prop), shdat.dat+SHMID_PROP, 0) < 0)
	goto open_failed;

    level_prop = shdat.dat[SHMID_PROP].ptr;

    if (level_prop->magic_no2 != MAP_MAGIC2 || level_prop->magic_no != MAP_MAGIC ||
	    level_prop->version_no != MAP_VERSION ||
	    level_prop->cells_x == 0 || level_prop->cells_y == 0 || level_prop->cells_z == 0)
	try_cw_file = 1;
    else
	// If it's not dirty, the cw file should be the master so okay to delete.
	if (!level_prop->dirty_save) {
	    char cwfilename[256];
	    snprintf(cwfilename, sizeof(cwfilename), LEVEL_CW_NAME, levelname);
	    if (access(cwfilename, R_OK) == 0)
		del_on_err = 1;
	}

    if (direct == 1) return;

    lock_shared();

    // Haven't got any reasonable level files, ok to overwrite.
    if (try_cw_file) {

	int ok = 0;
	// If level missing -- extract the matching *.cw file
	char cwfilename[256];
	snprintf(cwfilename, sizeof(cwfilename), LEVEL_CW_NAME, levelname);
	if (access(cwfilename, R_OK) == 0) {
	    ok = (load_map_from_file(cwfilename, levelname) >= 0);
	    // If the cw extraction fails we don't want to use anything
	    // else as overwriting it might be bad.
	    if (!ok)
		goto open_failed;

	    if (access(cwfilename, W_OK) != 0) {
		fprintf(stderr, "Loaded read only map %s\n", cwfilename);
		level_prop->readonly = 1;
	    }
	}

	// If level missing -- extract a model *.cw file
	if (!ok && access(MODEL_CW_NAME, R_OK) == 0) {
	    ok = (load_map_from_file(MODEL_CW_NAME, levelname) >= 0);
	}

	if (!ok) level_prop->version_no = 0;
    }

    // If level we still haven't got a level -- make a flat.
    if (level_prop->magic_no2 != MAP_MAGIC2 || level_prop->magic_no != MAP_MAGIC ||
	level_prop->version_no != MAP_VERSION ||
	level_prop->cells_x == 0 || level_prop->cells_y == 0 || level_prop->cells_z == 0)
    {
	printf_chat("&SLoading level file");
	flush_to_remote();
	createmap(levelname);
    } else
        // NB: Missing file here makes an Air map.
        if (open_blocks(levelname) < 0)
	    goto open_failed;

    if (level_prop->last_map_download_size <= 0) {
	level_prop->last_map_download_size = level_prop->total_blocks / 999;
	if (level_prop->last_map_download_size < 16384)
	    level_prop->last_map_download_size = 16384;
    }

    create_block_queue(levelname);
    if (!level_block_queue)
	goto open_failed;

    unlock_shared();
    return;

open_failed:
    stop_shared();
    if (del_on_err)
	unlink_level(levelname, 1);
}

int
open_blocks(char * levelname)
{
    char sharename[256];

    deallocate_shared(SHMID_BLOCKS);
    level_blocks = 0;

    sprintf(sharename, LEVEL_BLOCKS_NAME, levelname);
    int64_t l =
	    (int64_t)level_prop->cells_x * level_prop->cells_y *
	    level_prop->cells_z * sizeof(*level_blocks) +
	    sizeof(map_len_t);

    if((uintptr_t)l != l || (off_t)l != l || (size_t)l != l) {
	errno = EINVAL;
	perror("Server address space too small for this map");
	fatal("Map too large for this server compilation.");
    }
    if (allocate_shared(sharename, l, shdat.dat+SHMID_BLOCKS, 1) < 0)
	return -1;

    level_blocks = shdat.dat[SHMID_BLOCKS].ptr;
    return 0;
}

void
stop_shared(void)
{
    deallocate_shared(SHMID_PROP);
    deallocate_shared(SHMID_BLOCKS);
    level_prop = 0;
    level_blocks = 0;

    if (level_block_queue)
	stop_block_queue();
}

void
unlink_level(char * levelname, int silent)
{
    char sharename[256];
    check_level_name(levelname);

    snprintf(sharename, sizeof(sharename), LEVEL_PROPS_NAME, levelname);
    if (unlink(sharename) < 0 && !silent)
	perror(sharename);

    snprintf(sharename, sizeof(sharename), LEVEL_BLOCKS_NAME, levelname);
    if (unlink(sharename) < 0 && !silent)
	perror(sharename);

    snprintf(sharename, sizeof(sharename), LEVEL_QUEUE_NAME, levelname);
    if (unlink(sharename) < 0 && !silent)
	perror(sharename);
}

LOCAL void check_level_name(char * levelname)
{
    if (strchr(levelname, '/') != 0 || strlen(levelname) > 64) {
	char buf[256];
	snprintf(buf, sizeof(buf), "Illegal level name \"%.66s\"", levelname);
	fatal(buf);
        return;
    }
}

void
create_block_queue(char * levelname)
{
    if (!level_prop) return;

    char sharename[256];
    int queue_count = MIN_QUEUE;

    snprintf(sharename, sizeof(sharename), LEVEL_QUEUE_NAME, levelname);

    stop_block_queue();
    wipe_last_block_queue_id();

    // First minimum size.
    level_block_queue_len = sizeof(*level_block_queue) + queue_count * sizeof(xyzb_t);
    if (allocate_shared(sharename, level_block_queue_len, shdat.dat+SHMID_BLOCKQ, 1) < 0)
	return;
    level_block_queue = shdat.dat[SHMID_BLOCKQ].ptr;

    // Now try to extend it.
    lock_shared();

    // First init check
    if (level_block_queue->generation == 0 ||
	level_block_queue->queue_len < MIN_QUEUE ||
	level_block_queue->curr_offset >= level_block_queue->queue_len ||
	level_block_queue->queue_len > level_prop->total_blocks)
    {
	level_block_queue->generation += 2;
	level_block_queue->curr_offset = 0;
	level_block_queue->queue_len = queue_count;
    }

    // The file is this big.
    int file_queue_count = level_block_queue->queue_len;

    // We want it this big
    if (level_prop->last_map_download_size / msglen[PKID_SRVBLOCK] > queue_count) {
	queue_count = level_prop->last_map_download_size / msglen[PKID_SRVBLOCK];
	queue_count += 8;
	queue_count = (queue_count + 1024) / 1024 * 1024;
	if (queue_count < 2048) queue_count = 2048;
	queue_count -= 8;
    }

    // If it's sort of close, don't change it.
    if (file_queue_count > queue_count/2) queue_count = file_queue_count;

    stop_block_queue();

    // Now try the size we want.
    level_block_queue_len = sizeof(*level_block_queue) + queue_count * sizeof(xyzb_t);
    if (allocate_shared(sharename, level_block_queue_len, shdat.dat+SHMID_BLOCKQ, 1) < 0) {
	if (file_queue_count == queue_count)
	    fatal("Cannot open block queue");

	// Hummph. How about the size it is.
	level_block_queue_len = sizeof(*level_block_queue) + file_queue_count * sizeof(xyzb_t);
	if (allocate_shared(sharename, level_block_queue_len, shdat.dat+SHMID_BLOCKQ, 1) < 0)
	    fatal("Shared area allocation failure");

	queue_count = file_queue_count;
	level_block_queue = shdat.dat[SHMID_BLOCKQ].ptr;
    } else {
	level_block_queue = shdat.dat[SHMID_BLOCKQ].ptr;
	if (queue_count > level_block_queue->queue_len) {
	    level_block_queue->generation += 2;
	    level_block_queue->curr_offset = 0;
	    level_block_queue->queue_len = queue_count;
	}
    }

    unlock_shared();
}

void
stop_block_queue()
{
    if (level_block_queue) {
	deallocate_shared(SHMID_BLOCKQ);
	level_block_queue = 0;
	wipe_last_block_queue_id();
    }
}

void
open_client_list()
{
    char sharename[256];

    if (client_list) stop_client_list();

    sprintf(sharename, SYS_STAT_NAME);
    allocate_shared(sharename, sizeof(*client_list), shdat.dat+SHMID_CLIENTS, 0);
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
    deallocate_shared(SHMID_CLIENTS);
    client_list = 0;
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
    allocate_shared(sharename, level_chat_queue_len, shdat.dat+SHMID_CHAT, 0);
    level_chat_queue = shdat.dat[SHMID_CHAT].ptr;

    lock_chat_shared();
    if (    level_chat_queue->generation == 0 ||
	    level_chat_queue->generation > 0x0FFFFFFF ||
	    level_chat_queue->queue_len != queue_count ||
	    level_chat_queue->curr_offset >= queue_count) {
	level_chat_queue->generation = 2;
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
	deallocate_shared(SHMID_CHAT);
	level_chat_queue = 0;
	wipe_last_chat_queue_id();
    }
}

LOCAL int
allocate_shared(char * share_name, uintptr_t share_size, shmem_t *shm, int close_fd)
{
    uintptr_t sz;
    void * shm_p;
    int shared_fd = -1;

    // size_t for mmap and off_t for posix_fallocate
    assert((size_t)share_size > 0 && (off_t)share_size > 0);

    if (shm->lock_fd > 0) close(shm->lock_fd);
    shm->ptr = 0;
    shm->len = 0;
    shm->lock_fd = -1;

    shared_fd = open(share_name, O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC, 0600);
    if (shared_fd < 0) { perror(share_name); return -1; }

    {
	int p = sysconf(_SC_PAGESIZE);
	sz = share_size;
	sz += p - 1;
	sz -= sz % p;
    }

    // Weeeeeird calling process
    if ((errno = posix_fallocate(shared_fd, 0, sz)) != 0) {
	perror("posix_fallocate");
	close(shared_fd);
        return -1;
    }

    shm_p = (void*) mmap(0,
		    sz,
		    PROT_READ|PROT_WRITE,
		    MAP_SHARED,
		    shared_fd, 0);

    if ((intptr_t)shm_p == -1) {
	perror("mmap");
	close(shared_fd);
        return -1;
    }

    if (close_fd) {
	close(shared_fd);
	shared_fd = 0;
    }

    shm->ptr = shm_p;
    shm->len = sz;
    shm->lock_fd = shared_fd;

    return 0;
}

void
deallocate_shared(int share_id)
{
    if (shdat.dat[share_id].ptr) {
	void * p = shdat.dat[share_id].ptr;
	uintptr_t sz = shdat.dat[share_id].len;
	(void)munmap(p, sz);
	shdat.dat[share_id].ptr = 0;
	shdat.dat[share_id].len = 0;
    }
    if (shdat.dat[share_id].lock_fd > 0) {
	close(shdat.dat[share_id].lock_fd);
	shdat.dat[share_id].lock_fd = 0;
    }
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
   share_lock(chat_fd, F_SETLKW, F_WRLCK);
}

void
unlock_chat_shared(void)
{
   share_lock(chat_fd, F_SETLK, F_UNLCK);
}

void
lock_client_data(void)
{
   share_lock(client_fd, F_SETLKW, F_WRLCK);
}

void
unlock_client_data(void)
{
   share_lock(client_fd, F_SETLK, F_UNLCK);
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

