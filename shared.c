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
 *       POSIX Semephores do NOT release on process termination. (sem_wait(1))
 */

#include "shared.h"

#if INTERFACE
#include <stdint.h>

#define SHMID_PROP	0
#define SHMID_BLOCKS	1
#define SHMID_BLOCKQ	2
#define SHMID_CHAT	3
#define SHMID_CLIENTS	4
#define SHMID_SYSCONF	5
#define SHMID_COUNT	6

typedef struct shmem_t shmem_t;
struct shmem_t {
    void * ptr;
    uintptr_t len;
    int lock_fd;
};

typedef struct shared_data_t shared_data_t;
struct shared_data_t {
    map_info_t *prop;
    block_t *blocks;
    block_queue_t* blockq;
    chat_queue_t *chat;
    client_data_t *client;

    uint32_t block_queue_mmap_count;
    char * level_fixed_name;

    shmem_t dat[SHMID_COUNT];
};

#define level_prop shdat.prop
#define level_blocks shdat.blocks
#define level_block_queue shdat.blockq
#define level_chat_queue shdat.chat
#endif

#define level_block_queue_len shdat.dat[SHMID_BLOCKQ].len
#define level_chat_queue_len shdat.dat[SHMID_CHAT].len

struct shared_data_t shdat;

filelock_t chat_queue_lock[1] = {{.name = CHAT_LOCK_NAME}};
filelock_t level_lock[1];

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
 * open the level/$level.* files, if they don't exist they will be
 * created, usually from a matching map/$level.cw file.
 *
 * to_unload:
 *    ==0: open or create
 *    !=0: just open files don't create
 */
void
open_level_files(char * level_name, int backup_id, char * fixname, int to_unload)
{
    char sharename[256];
    int del_on_err = 0, try_cw_file = 0;

    stop_shared();

    check_level_name(fixname); // Last check.
    shdat.level_fixed_name = strdup(fixname);

    saprintf(sharename, LEVEL_LOCK_NAME, fixname);
    level_lock->name = strdup(sharename);
    lock_start(level_lock);

    saprintf(sharename, LEVEL_BLOCKS_NAME, fixname);
    del_on_err = access(sharename, F_OK);
    saprintf(sharename, LEVEL_PROPS_NAME, fixname);
    if (del_on_err == 0)
	del_on_err = access(sharename, F_OK);
    if (del_on_err < 0) {
	if(errno == EACCES) del_on_err = 0;
	else del_on_err = 1;
    } else del_on_err = 0;

    if (del_on_err && to_unload) return;

    lock_fn(level_lock);

    if (allocate_shared(sharename, sizeof(*level_prop), shdat.dat+SHMID_PROP) < 0)
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
	    saprintf(cwfilename, LEVEL_CW_NAME, fixname);
	    if (access(cwfilename, R_OK) == 0)
		del_on_err = 1;
	}

    // Haven't got any reasonable level files, ok to overwrite.
    if (try_cw_file) {

	int ok = 0;
	// If level missing -- extract the matching *.cw file
	char cwfilename[256];
	if (backup_id == 0)
	    saprintf(cwfilename, LEVEL_CW_NAME, fixname);
	else
	    saprintf(cwfilename, LEVEL_BACKUP_DIR_NAME "/%s.cw", fixname);
	if (access(cwfilename, R_OK) == 0) {
	    ok = (load_map_from_file(cwfilename, fixname, level_name) >= 0);
	    // If the cw extraction fails we don't want to use anything
	    // else as overwriting it might be bad.
	    if (!ok)
		goto open_failed;

	    if (access(cwfilename, W_OK) != 0) {
		printlog("Loaded read only map %s", cwfilename);
		level_prop->readonly = 1;
	    }
	}

	if (!ok) level_prop->version_no = 0;
    }

    // If level we still haven't got a level -- make a flat.
    if (level_prop->magic_no2 != MAP_MAGIC2 || level_prop->magic_no != MAP_MAGIC ||
	level_prop->version_no != MAP_VERSION ||
	level_prop->cells_x == 0 || level_prop->cells_y == 0 || level_prop->cells_z == 0)
    {
	fprintf_logfile("Level \"%s\" does not have valid file, creating map", level_name);
	createmap(fixname);
    } else
        // NB: Missing file here makes an Air map.
        if (open_blocks(fixname) < 0)
	    goto open_failed;

    if (level_prop->last_map_download_size <= 0) {
	level_prop->last_map_download_size = level_prop->total_blocks / 999;
	if (level_prop->last_map_download_size < 16384)
	    level_prop->last_map_download_size = 16384;
    }

    wipe_last_block_queue_id();
    create_block_queue(fixname);
    if (!level_block_queue)
	goto open_failed;

    unlock_fn(level_lock);
    return;

open_failed:
    stop_shared();
    if (del_on_err) {
	unlink_level(fixname, 1);
	// NB: Cannot safely removed the lock file without checking for
	//     other users (nearly) on this level.
    }
}

void
create_property_file(char * level_name, char * fixname)
{
    stop_shared_mmap();

    char sharename[256];
    saprintf(sharename, LEVEL_PROPS_NAME, fixname);
    if (allocate_shared(sharename, sizeof(*level_prop), shdat.dat+SHMID_PROP) < 0) {
	printf_chat("&WLevel open failed for %s", level_name);
	return;
    }

    level_prop = shdat.dat[SHMID_PROP].ptr;
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
	printf_chat("Map is too large (%d,%d,%d) -> %jd cells",
	    level_prop->cells_x, level_prop->cells_y, level_prop->cells_z,
	    (intmax_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z);
	return -1;
    }
    if (allocate_shared(sharename, l, shdat.dat+SHMID_BLOCKS) < 0)
	return -1;

    level_blocks = shdat.dat[SHMID_BLOCKS].ptr;
    return 0;
}

void
stop_shared(void)
{
    stop_shared_mmap();
    stop_shared_lock();
    shdat.block_queue_mmap_count = 0;
    if (shdat.level_fixed_name) {
	free(shdat.level_fixed_name);
	shdat.level_fixed_name = 0;
    }
}

void
stop_shared_mmap(void)
{
    deallocate_shared(SHMID_PROP);
    deallocate_shared(SHMID_BLOCKS);
    level_prop = 0;
    level_blocks = 0;

    if (level_block_queue)
	stop_block_queue();
}

void
stop_shared_lock(void)
{
    lock_stop(level_lock);
    if (level_lock->name) { free(level_lock->name); level_lock->name = 0; }
}

void
unlink_level(char * levelname, int silent)
{
    char sharename[256];

    saprintf(sharename, LEVEL_PROPS_NAME, levelname);
    if (access(sharename, F_OK) == 0)
	if (unlink(sharename) < 0 && !silent)
	    perror(sharename);

    saprintf(sharename, LEVEL_BLOCKS_NAME, levelname);
    if (access(sharename, F_OK) == 0)
	if (unlink(sharename) < 0 && !silent)
	    perror(sharename);

    saprintf(sharename, LEVEL_QUEUE_NAME, levelname);
    if (access(sharename, F_OK) == 0)
	if (unlink(sharename) < 0 && !silent)
	    perror(sharename);
}

LOCAL void check_level_name(char * levelname)
{
    if (strchr(levelname, '/') != 0 || levelname[0] == '.'
	|| strlen(levelname) > MAXLEVELNAMELEN*4) {
	char buf[256];
	saprintf(buf, "Illegal level file name \"%.66s\"", levelname);
	fatal(buf);
    }
}

LOCAL void
create_block_queue(char * levelname)
{
    stop_block_queue();

    if (!level_prop) return;

    char sharename[256];
    int queue_count = MIN_QUEUE;

    saprintf(sharename, LEVEL_QUEUE_NAME, levelname);

    // First minimum size.
    level_block_queue_len = sizeof(*level_block_queue) + queue_count * sizeof(xyzb_t);
    if (allocate_shared(sharename, level_block_queue_len, shdat.dat+SHMID_BLOCKQ) < 0)
	return;
    level_block_queue = shdat.dat[SHMID_BLOCKQ].ptr;

    // First init check
    if (level_block_queue->generation == 0 ||
	level_block_queue->queue_len < MIN_QUEUE ||
	level_block_queue->curr_offset >= level_block_queue->queue_len ||
	(level_block_queue->queue_len > STD_QUEUE && level_block_queue->queue_len > level_prop->total_blocks))
    {
	if (level_block_queue->queue_len != 0)
	    printlog("(Re-)Init queue g=%d, q=%d, tb=%d",
		level_block_queue->generation,
		level_block_queue->queue_len,
		(int)level_prop->total_blocks);

	level_block_queue->generation += 2;
	level_block_queue->curr_offset = 0;
	level_block_queue->queue_len = queue_count;
	level_block_queue->last_queue_len = queue_count;
    }

    // The file is this big.
    int file_queue_count = level_block_queue->queue_len;

    // We want it this big
    if (level_prop->last_map_download_size / msglen[PKID_SRVBLOCK] > queue_count) {
	queue_count = level_prop->last_map_download_size / msglen[PKID_SRVBLOCK];
	queue_count += 8;
	queue_count = (queue_count + 1024) / 1024 * 1024;
	if (queue_count < 2048) queue_count = STD_QUEUE;
	queue_count -= 8;
    }

    // If it's more, don't change it.
    if (file_queue_count > queue_count) queue_count = file_queue_count;

    // fprintf(stderr, "Trace Open queue %d,%d to %d '%s'\n", file_queue_count, shdat.block_queue_mmap_count, queue_count, shdat.level_fixed_name);

    stop_block_queue();

    // Now try the size we want.
    level_block_queue_len = sizeof(*level_block_queue) + queue_count * sizeof(xyzb_t);
    if (allocate_shared(sharename, level_block_queue_len, shdat.dat+SHMID_BLOCKQ) < 0) {
	if (file_queue_count == queue_count)
	    fatal("Cannot open block queue");

	// Hummph. How about the size it is.
	queue_count = file_queue_count;
	level_block_queue_len = sizeof(*level_block_queue) + queue_count * sizeof(xyzb_t);
	if (allocate_shared(sharename, level_block_queue_len, shdat.dat+SHMID_BLOCKQ) < 0)
	    fatal("Shared area allocation failure");

	level_block_queue = shdat.dat[SHMID_BLOCKQ].ptr;
    } else {
	level_block_queue = shdat.dat[SHMID_BLOCKQ].ptr;
	if (queue_count > level_block_queue->queue_len)
	    level_block_queue->queue_len = queue_count;
    }

    if (level_block_queue->last_queue_len == 0)
	level_block_queue->last_queue_len = queue_count;

    shdat.block_queue_mmap_count = queue_count;
}

void
stop_block_queue()
{
    if (level_block_queue) {
	deallocate_shared(SHMID_BLOCKQ);
	level_block_queue = 0;
    }
}

int
check_block_queue(int need_lock)
{
    if (!level_block_queue) return 0;
    if (shdat.block_queue_mmap_count == level_block_queue->queue_len) return 0;
    if (shdat.block_queue_mmap_count > level_block_queue->queue_len) {
	// fatal("level_block_queue->queue_len shrank!?");
	shdat.block_queue_mmap_count = level_block_queue->queue_len;
	return 0;
    }
    if (shdat.level_fixed_name == 0)
	fatal("Cannot reopen queue, name lost");

    if (need_lock) lock_fn(level_lock);
    create_block_queue(shdat.level_fixed_name);
    if (need_lock) unlock_fn(level_lock);

    if (!level_block_queue)
	fatal("Block queue check lost queue");

    return 1;
}

void
open_client_list()
{
    if (shdat.client) return;

    char sharename[256];
    sprintf(sharename, SYS_STAT_NAME);
    allocate_shared(sharename, sizeof(*shdat.client), shdat.dat+SHMID_CLIENTS);
    shdat.client = shdat.dat[SHMID_CLIENTS].ptr;
    if (!shdat.client) return;

    if (shdat.client->magic1 != MAGIC_USR || shdat.client->magic2 != MAGIC_USR) {
	client_data_t d = { .magic1 = MAGIC_USR, .magic2 = MAGIC_USR };
	*shdat.client = d;
    }
}

void
stop_client_list()
{
    deallocate_shared(SHMID_CLIENTS);
    shdat.client = 0;
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

    lock_start(chat_queue_lock);
    lock_fn(chat_queue_lock);

    sprintf(sharename, CHAT_QUEUE_NAME);
    level_chat_queue_len = sizeof(*level_chat_queue) + queue_count * sizeof(chat_entry_t);
    allocate_shared(sharename, level_chat_queue_len, shdat.dat+SHMID_CHAT);
    level_chat_queue = shdat.dat[SHMID_CHAT].ptr;

    if (    level_chat_queue->generation == 0 ||
	    level_chat_queue->generation > 0x0FFFFFFF ||
	    level_chat_queue->queue_len != queue_count ||
	    level_chat_queue->curr_offset >= queue_count) {
	level_chat_queue->generation = 2;
	level_chat_queue->curr_offset = 0;
	level_chat_queue->queue_len = queue_count;
    }
    unlock_fn(chat_queue_lock);

    set_last_chat_queue_id();
}

void
stop_chat_queue()
{
    if (level_chat_queue) {
	deallocate_shared(SHMID_CHAT);
	level_chat_queue = 0;
	wipe_last_chat_queue_id();
	lock_stop(chat_queue_lock);
    }
}

void
open_system_conf()
{
    char sharename[256];

    // if (server) stop_system_conf();
    if (server) return;

    sprintf(sharename, SYS_CONF_NAME);
    allocate_shared(sharename, sizeof(*server), shdat.dat+SHMID_SYSCONF);
    server = shdat.dat[SHMID_SYSCONF].ptr;

    if (!server) {
	perror("Opening server.dat file");
	exit(1);
    }
}

void
stop_system_conf()
{
    if (server) {
	deallocate_shared(SHMID_SYSCONF);
	server = 0;
    }
}

// These flags are not strictly required, but probably a good idea
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

LOCAL int
allocate_shared(char * share_name, uintptr_t share_size, shmem_t *shm)
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

    close(shared_fd);
    shared_fd = 0;

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
