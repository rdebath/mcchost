#include <sys/mman.h>
#include <fcntl.h>
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

#define MAXLEVELNAMELEN 32

#define SHMID_PROP	0
#define SHMID_BLOCKS	1
#define SHMID_BLOCKQ	2
#define SHMID_CHAT	3
#define SHMID_CMD	4
#define SHMID_CLIENTS	5
#define SHMID_SYSCONF	6
#define SHMID_TMPBLOCKS	7
#define SHMID_COUNT	8

typedef struct shmem_t shmem_t;
struct shmem_t {
    void * ptr;
    uintptr_t len;
    int zeroed;
    int lock_fd;
};

typedef struct shared_data_t shared_data_t;
struct shared_data_t {
    map_info_t *prop;
    map_block_t *blocks;
    block_queue_t* blockq;
    client_data_t *client;

    uint32_t block_queue_mmap_count;
    char * level_fixed_name;

    shmem_t dat[SHMID_COUNT];
};

#define level_prop shdat.prop
#define level_blocks shdat.blocks
#define level_blocks_zeroed (shdat.dat[SHMID_BLOCKS].zeroed)
#define level_block_queue shdat.blockq
#endif

#if _POSIX_VERSION >= 200112L && _POSIX_ADVISORY_INFO >= 200112L
#define USE_POSIXALLOC
#else
#warning posix_fallocate not available, using fallback
#endif

#define level_block_queue_len shdat.dat[SHMID_BLOCKQ].len

chat_queue_t * shared_chat_queue = 0;
cmd_queue_t * shared_cmd_queue = 0;

struct shared_data_t shdat;

filelock_t chat_queue_lock[1] = {{.name = CHAT_LOCK_NAME}};
filelock_t cmd_queue_lock[1] = {{.name = CMD_LOCK_NAME}};
filelock_t level_lock[1];

/*****************************************************************************
 * Shared resources.
 */

/*
    Items:
	level_queue -- recent block changes broadcast

	chat_queue -- chat messages broadcast

	user_chat_queue -- chat messages for one user
	shared_chat_queue -- chat messages for one level broadcast
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
open_level_files(char * level_name, int backup_id, char * cw_name, char * fixname, int to_unload)
{
    char sharename[256];
    int del_on_err = 0, try_cw_file = 0;

    stop_shared();

    char cwfilename[256];

    if (cw_name == 0) {
	cw_name = cwfilename;
	if (backup_id == 0)
	    saprintf(cwfilename, LEVEL_CW_NAME, fixname);
	else
	    saprintf(cwfilename, LEVEL_BACKUP_DIR_NAME "/%s.cw", fixname);
    }

    if (!check_level_name(fixname)) {
	printlog("Failed attempt to open invalid level name \"%s\"", fixname);
	return;
    }

    shdat.level_fixed_name = strdup(fixname);

    saprintf(sharename, LEVEL_LOCK_NAME, fixname);
    level_lock->name = strdup(sharename);
    if (!lock_start_try(level_lock)) {
	fprintf_logfile("Unable to lock level %s", level_name);
	return;
    }

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

    if (level_prop->magic_no2 != TY_MAGIC2 || level_prop->magic_no != TY_MAGIC ||
	    level_prop->version_no != TY_LVERSION ||
	    level_prop->cells_x == 0 || level_prop->cells_y == 0 || level_prop->cells_z == 0)
	try_cw_file = 1;
    else
	// If it's not dirty, the cw file should be the master so okay to delete.
	if (!level_prop->dirty_save) {
	    if (access(cw_name, R_OK) == 0)
		del_on_err = 1;
	}

    // Haven't got any reasonable level files, ok to overwrite.
    if (try_cw_file) {

	int ok = 0;
	// If level missing -- extract the matching *.cw file
	if (access(cw_name, R_OK) == 0) {
	    ok = (load_map_from_file(cw_name, fixname, level_name, backup_id) >= 0);
	    // If the cw extraction fails we don't want to use anything
	    // else as overwriting it might be bad.
	    if (!ok) {
		printlog("Load of level map file \"%s\" failed", cw_name);
		goto open_failed;
	    }

	    if (access(cw_name, W_OK) != 0) {
		printlog("Loaded read only map \"%s\"", cw_name);
		level_prop->readonly = 1;
	    }
	}

	if (!ok) level_prop->version_no = 0;
    }

    // If level we still haven't got a level -- make a flat.
    if (level_prop->magic_no2 != TY_MAGIC2 || level_prop->magic_no != TY_MAGIC ||
	level_prop->version_no != TY_LVERSION ||
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
	printf_chat("#&WLevel open failed for %s", level_name);
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

    if(*levelname == 0) return -1;
    sprintf(sharename, LEVEL_BLOCKS_NAME, levelname);
    int64_t l =
	    (int64_t)level_prop->cells_x * level_prop->cells_y *
	    level_prop->cells_z * sizeof(*level_blocks) +
	    sizeof(map_len_t);

    if((uintptr_t)l != l || (off_t)l != l || (size_t)l != l) {
	errno = EINVAL;
	perror("Server address space too small for this map");
	printf_chat("#Map \"%s\" is too large (%d,%d,%d) -> %jd cells",
	    levelname,
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

int
open_tmp_blocks(char * levelname, int cells_x, int cells_y, int cells_z)
{
    char sharename[256];

    deallocate_shared(SHMID_TMPBLOCKS);

    if(*levelname == 0) return -1;
    sprintf(sharename, LEVEL_TMPBLOCKS_NAME, levelname);
    int64_t l = (int64_t)cells_x * cells_y * cells_z * sizeof(*level_blocks) + sizeof(map_len_t);

    if((uintptr_t)l != l || (off_t)l != l || (size_t)l != l) {
	errno = EINVAL;
	perror("Server address space too small for this map");
	printf_chat("#Map \"%s\" is too large (%d,%d,%d) -> %jd cells",
	    levelname, cells_x, cells_y, cells_z, (intmax_t)cells_x * cells_y * cells_z);
	return -1;
    }
    if (allocate_shared(sharename, l, shdat.dat+SHMID_TMPBLOCKS) < 0)
	return -1;

    //level_tmpblocks = shdat.dat[SHMID_TMPBLOCKS].ptr;
    return 0;
}

void
stop_tmp_blocks(void)
{
    deallocate_shared(SHMID_TMPBLOCKS);
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

LOCAL int check_level_name(char * levelname)
{
    if (strchr(levelname, '/') != 0 || levelname[0] == '.' || levelname[0] == 0
	|| strlen(levelname) >= MAXLEVELNAMELEN*4)
    {
	return 0;
    }
    return 1;
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
	    printlog("Corruption? (Re-)Init block queue g=%d, q=%d, tb=%d",
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

    if (shdat.client->magic_no != TY_MAGIC
	|| shdat.client->magic_sz != TY_MAGIC3
	|| shdat.client->magic2 != TY_MAGIC2
	|| shdat.client->version != TY_CVERSION) {
	client_data_t d = { .magic_no = TY_MAGIC, .magic_sz = TY_MAGIC3, .magic2 = TY_MAGIC2, .version = TY_CVERSION };
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
    int cqlen = CHAT_QUEUE_LEN;

    stop_chat_queue();
    wipe_last_chat_queue_id();

    lock_start(chat_queue_lock);
    lock_fn(chat_queue_lock);

    sprintf(sharename, CHAT_QUEUE_NAME);
    shdat.dat[SHMID_CHAT].len = sizeof(*shared_chat_queue);
    allocate_shared(sharename, shdat.dat[SHMID_CHAT].len, shdat.dat+SHMID_CHAT);
    shared_chat_queue = shdat.dat[SHMID_CHAT].ptr;

    if (    shared_chat_queue->generation == 0 ||
	    shared_chat_queue->generation > 0x0FFFFFFF ||
	    shared_chat_queue->queue_len != cqlen ||
	    shared_chat_queue->curr_offset >= cqlen) {
	shared_chat_queue->generation = 2;
	shared_chat_queue->curr_offset = 0;
	shared_chat_queue->queue_len = cqlen;
    }
    unlock_fn(chat_queue_lock);

    set_last_chat_queue_id();
}

void
stop_chat_queue()
{
    if (shared_chat_queue) {
	deallocate_shared(SHMID_CHAT);
	shared_chat_queue = 0;
	wipe_last_chat_queue_id();
	lock_stop(chat_queue_lock);
    }
}

void
create_cmd_queue()
{
    char sharename[256];
    int cqlen = CMD_QUEUE_LEN;

    stop_cmd_queue();
    wipe_last_cmd_queue_id();

    lock_start(cmd_queue_lock);
    lock_fn(cmd_queue_lock);

    sprintf(sharename, CMD_QUEUE_NAME);
    shdat.dat[SHMID_CMD].len = sizeof(*shared_cmd_queue);
    allocate_shared(sharename, shdat.dat[SHMID_CMD].len, shdat.dat+SHMID_CMD);
    shared_cmd_queue = shdat.dat[SHMID_CMD].ptr;

    if (    shared_cmd_queue->generation == 0 ||
	    shared_cmd_queue->generation > 0x0FFFFFFF ||
	    shared_cmd_queue->queue_len != cqlen ||
	    shared_cmd_queue->curr_offset >= cqlen) {
	shared_cmd_queue->generation = 2;
	shared_cmd_queue->curr_offset = 0;
	shared_cmd_queue->queue_len = cqlen;
    }
    unlock_fn(cmd_queue_lock);

    set_last_cmd_queue_id();
}

void
stop_cmd_queue()
{
    if (shared_cmd_queue) {
	deallocate_shared(SHMID_CMD);
	shared_cmd_queue = 0;
	wipe_last_cmd_queue_id();
	lock_stop(cmd_queue_lock);
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

    if (ini_settings != &server_ini_settings)
	ini_settings = &server->shared_ini_settings;

    if (!server) {
	perror("Opening server.dat file");
	exit(1);
    }
}

void
stop_system_conf()
{
    if (server) {
	if (ini_settings != &server_ini_settings)
	    ini_settings = 0;
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
    struct timeval start;
    gettimeofday(&start, 0);

    // size_t for mmap and off_t for posix_fallocate
    assert((size_t)share_size > 0 && (off_t)share_size > 0);

    if (shm->lock_fd > 0) close(shm->lock_fd);
    shm->ptr = 0;
    shm->len = 0;
    shm->zeroed = 0;
    shm->lock_fd = -1;

    shared_fd = open(share_name, O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC, 0600);
    if (shared_fd < 0) { perror(share_name); return -1; }

    // Ensure the file is the right size and allocated.
    {
	struct stat st;
	if (fstat(shared_fd, &st) == 0 && st.st_size == 0)
	    shm->zeroed = 1;	// System will do this.

	int p = sysconf(_SC_PAGESIZE);
	sz = share_size;
	sz += p - 1;
	sz -= sz % p;

	if (st.st_size != sz)
	{
	    // On a current local Linux (filesystem) this is always fast.
	    // Others (Network and non-Linux fs) might take a while.
#ifdef USE_POSIXALLOC
	    // Weeeeeird calling process
	    if ((errno = posix_fallocate(shared_fd, 0, sz)) != 0)
#else
	    if (ftruncate(shared_fd, sz) < 0)
#endif
	    {
		perror("fallocate/ftruncate");
		close(shared_fd);
		return -1;
	    }
	}
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

    struct timeval now;
    gettimeofday(&now, 0);
    double ms = (now.tv_sec-start.tv_sec)*1000.0+(now.tv_usec-start.tv_usec)/1000.0;
    if (ms > 100.0)
	printlog("WARNING: Mapping of \"%s\" took %s is it on a local filesystem?",
	    share_name, conv_ms_a(ms));

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
