#include <stdio.h>
#include <stdlib.h>
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
 *       fcntl() does this, but is slow.
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

#if INTERFACE

// Is -pthreads option provided.
#ifndef _REENTRANT
#define USE_FCNTL
#endif

#ifndef USE_FCNTL
#include <semaphore.h>
#define lock_shared() sem_wait(shared_global_mutex);
#define unlock_shared() sem_post(shared_global_mutex);
#endif

#endif

volatile map_info_t *level_prop = 0;
intptr_t level_prop_len = 0;

volatile block_t *level_blocks = 0;
intptr_t level_blocks_len = 0;

#ifdef USE_FCNTL
static int fcntl_fd = -1;
#else
sem_t* shared_global_mutex = 0;
#endif


/*****************************************************************************
 * Shared resources.
 */

/*
    TODO:
	level_queue -- recent block changes broadcast

	chat_queue -- chat messages broadcast
	user_chat_queue -- chat messages for one user
	level_chat_queue -- chat messages for one level broadcast
	team_chat_queue -- chat messages for one team broadcast
	    --> Are these all in same queue, just filtered?

	server_prop -- properties of server, not linked to user or level.
	    --> Is this a text file?

	user_prop -- User properties
	    --> Text file? Blob in data table?
 */

void
start_shared(char * levelname)
{
    char sharename[256];
    if (level_prop && level_blocks) return;

    stop_shared();

    sprintf(sharename, "level.%s.prop", levelname);
    level_prop = allocate_shared(sharename, sizeof(*level_prop), &level_prop_len);

#ifdef USE_FCNTL
    fcntl_fd = open(sharename, O_RDWR|O_NOFOLLOW);
    if (fcntl_fd < 0) {
	fprintf(stderr, "Cannot open \"%s\" for locking\n", sharename);
	exit(2);
    }
#else
    {
	char namebuf[512];
	sprintf(namebuf, "/global.%s", sharename);
	shared_global_mutex = sem_open(namebuf, O_CREAT, 0777, 1);
    }
#endif

    lock_shared();

    // If level missing -- extract the matching *.cw file

    // If level missing -- extract a model *.cw file

    // If level missing -- make a flat.
    if (level_prop->cells_x == 0 || level_prop->cells_y == 0 || level_prop->cells_z == 0) {
	map_info_t def = {
		.magic_no = MAP_MAGIC,
		.cells_x = 128, .cells_y = 64, .cells_z = 128,
		.weather = 0, -1, -1, -1, -1, -1, 7, 8, -1,
		.spawn = { 64*32+16, 48*32+51, 64*32+16 }
	    };

	*level_prop = def;

	// Should write() blocks here so that file can be dropped on ENOSPACE
    }

    sprintf(sharename, "level.%s.blks", levelname);
    level_blocks_len = level_prop->cells_x * level_prop->cells_y * level_prop->cells_z * sizeof(*level_blocks);
    level_blocks = allocate_shared(sharename, level_blocks_len, &level_blocks_len);

    // If level not valid wipe to flat.
    if (level_prop->valid_blocks == 0) {
	int x, y, z, y1;
	y1 = level_prop->cells_y/2;
	for(y=0; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		{
		    if (y>y1)
			level_blocks[World_Pack(x,y,z)] = 0;
		    else if (y==y1)
			level_blocks[World_Pack(x,y,z)] = Block_Grass;
		    else
			level_blocks[World_Pack(x,y,z)] = Block_Dirt;
		}
	level_prop->valid_blocks = VALID_MAGIC;
    }

    unlock_shared();
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

#ifdef USE_FCNTL
    if (fcntl_fd != -1) {
	close(fcntl_fd);
	fcntl_fd = -1;
    }
#endif
}

LOCAL void *
allocate_shared(char * share_name, int share_size, intptr_t *shared_len)
{
    intptr_t sz;
    void *shared_mem = 0;
    void * shm_p;
    int shared_fd = -1;

    /* GLobal file for global locking. */
    if (strchr(share_name, '/') != 0 || strlen(share_name) > 128) {
	fprintf(stderr, "Illegal share name \"%s\"\n", share_name);
	exit(2);
    }

    shared_fd = open(share_name, O_CREAT|O_RDWR|O_NOFOLLOW, 0600);
    if (shared_fd < 0) {
	fprintf(stderr, "Cannot open \"%s\" for mmap\n", share_name);
	exit(2);
    }

    {
	int p = sysconf(_SC_PAGESIZE);
	sz = share_size;
	sz += p - 1;
	sz -= sz % p;
    }

    if (ftruncate(shared_fd, sz) < 0)
	perror("ftruncate");

    shm_p = (void*) mmap(0,
		    sz,
		    PROT_READ|PROT_WRITE,
		    MAP_SHARED,
		    shared_fd, 0);

    if ((intptr_t)shm_p == -1)
	fatal("Cannot allocate shared area");

    shared_mem = shm_p;
    *shared_len = sz;

    close(shared_fd);

    return shared_mem;
}

#ifdef USE_FCNTL
static void
share_lock(int mode, int l_type)
{
   struct flock lck;
   int rv;

   for(;;)
   {
      lck.l_type = l_type;
      lck.l_whence = SEEK_SET;
      lck.l_start = 0;
      lck.l_len = 0;

      rv = fcntl(fcntl_fd, mode, &lck);
      if (rv >= 0) return;
      if (rv < 0 && errno != EINTR) {
	 perror("Problem locking shared area");
	 return;
      }
   }
}

void
lock_shared(void)
{
   share_lock(F_SETLKW, F_WRLCK);
}

void
unlock_shared(void)
{
   share_lock(F_SETLK, F_UNLCK);
}
#endif
