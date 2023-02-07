
// Failed on MacOSX and FreeBSD
//
// MacOSX does not have PTHREAD_MUTEX_ROBUST
//
// FreeBSD has it, but it doesn't work.
//  -> pthread_mutex_t is a pointer and so cannot exist in real shared memory.

#if _REENTRANT
// The define _REENTRANT must be defined by -pthread for normal compilers.
// Occasionally, you will find a buggy compiler that doesn't define this
// and you'll have to use "PTHREAD='-pthread -D_REENTRANT'" to workaround
// the bug. However, these compilers are likely to have broken pthreads
// anyway so you'll probably have to do "PTHREAD=" instead to disable
// this option and use fcntl().

// The DISABLE_ROBUST flag is for the rare pthreads implementation that
// correctly implements robust multi-process locks by default.
// Please use the test program at the end of this file check for this.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "pthread_mutex.h"

#if INTERFACE
#include <pthread.h>
#define HAS_PTHREAD

typedef struct lock_shm_t lock_shm_t;
struct lock_shm_t {
    int64_t magic;
    pthread_mutex_t mutex[1];
};

#define filelock_t pthread_filelock_t
typedef struct pthread_filelock_t pthread_filelock_t;
struct pthread_filelock_t {
    char * name;
    int have_lock; // Not allowed to "nest" pthread locks, but can't
		   // portably configure PTHREAD_MUTEX_ERRORCHECK
    struct lock_shm_t * lock;
};
#endif

// This MUST be compiled with -pthread otherwise gcc makes dumb assumptions.
#if defined(HAS_PTHREAD)

#define E(_x) ERR_PT((_x), #_x, __FILE__, __LINE__)
static inline int ERR_PT(int n, char * tn, char *fn, int ln)
{
    if (n == 0 || n == EOWNERDEAD) return n;
    fprintf(stderr, "%s:%d: %s: %s\n", fn, ln, tn, strerror(n)),
    abort();
}

void
lock_fn(filelock_t * ln)
{
    if (!ln->lock || ln->have_lock) return;

    if (E(pthread_mutex_lock(ln->lock->mutex)))
#ifndef DISABLE_ROBUST
	E(pthread_mutex_consistent(ln->lock->mutex));
#else
	abort();
#endif
    ln->have_lock = 1;
}

void
unlock_fn(filelock_t * ln)
{
    if (!ln->lock || !ln->have_lock) return;
    E(pthread_mutex_unlock(ln->lock->mutex));
    ln->have_lock = 0;
}

#if INTERFACE
inline static void
lock_start(pthread_filelock_t * ln)
{
    if (!lock_start_try(ln)) abort();
}
#endif

int
lock_start_try(filelock_t * ln)
{
    int fd = -1;

    // There is no safe way to create a pthread mutex when it may be in use.

    // This routine uses "primitive" unix file locking to ensure that one
    // process can call pthread_mutexattr_init without any other process
    // trying to actually use the mutex.  The lockfile is created as
    // a process specific file using getpid() and once it's valid is
    // put in place using an atomic link(). If the link fails (EEXIST)
    // another process has created the file and we we discard our attempt.

    int ecount = 0;
    ln->lock = 0;
    if (!ln->name) return 0;

    while(1)
    {
	if (errno != 0 && ecount > 20) { perror(ln->name); return 0; }

	fd = open(ln->name, O_RDWR|O_CLOEXEC);
	if (fd>=0) {
	    struct stat st = {0};
	    if (fstat(fd, &st) < 0)
		st.st_size = 0;

	    if (st.st_size >= sizeof(pthread_mutex_t)) {
		lock_shm_t * tmp_lock;
		tmp_lock = mmap(NULL, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		if (tmp_lock != (void *) -1)
		    ln->lock = tmp_lock;

		if (ln->lock && ln->lock->magic == TY_MAGIC) {
		    // Try to lock the lock; bad errors mean we recreate it.
		    errno = 0;
		    int n = pthread_mutex_trylock(ln->lock->mutex);
		    if (n == EBUSY) return 1; // In use is fine.
#ifndef DISABLE_ROBUST
		    if (n == EOWNERDEAD) {
			n = pthread_mutex_consistent(ln->lock->mutex);
			if (n != 0)
			    fprintf(stderr, "Lockfile \"%s\" inconsistent -> %s\n", ln->name, strerror(n));
		    }
#endif
		    if (n == 0) {
			pthread_mutex_unlock(ln->lock->mutex);
			return 1;
		    }
		    if (n == EINVAL)
			fprintf(stderr, "Lockfile \"%s\" returned EINVAL -> %s\n", ln->name, strerror(errno));
		    else
			fprintf(stderr, "Lockfile \"%s\" returned %d -> %s\n", ln->name, n, strerror(n));
		} else
		    fprintf(stderr, "ERROR: Bad magic in lock file \"%s\" 0%jo, requires recreation\n",
			ln->name, (uintmax_t)ln->lock->magic);

		lock_stop(ln);
	    } else
		close(fd);
	    ecount++;

	    // This is dangerous, but should never happen unless someone has
	    // corrupted the lock file.
	    // Hopefully this delay is long enough for everyone to be happy.
	    if (ecount > 20) { perror(ln->name); return 0; }
	    if (unlink(ln->name) == 0)
		msleep(500);
	    continue;
	}

	// ENOENT -> Need to make; other -> not good.
	if (errno != ENOENT || ecount > 100) { perror(ln->name); return 0; }
	ecount++;

	// Create the mutex in local memory
	pthread_mutexattr_t att;
	lock_shm_t lock0 = {.magic=TY_MAGIC};
	E(pthread_mutexattr_init(&att));
	E(pthread_mutexattr_setpshared(&att, PTHREAD_PROCESS_SHARED));
#ifndef DISABLE_ROBUST /* PTHREAD_MUTEX_ROBUST is not visible to ifdef */
	E(pthread_mutexattr_setrobust(&att, PTHREAD_MUTEX_ROBUST));
#endif
	E(pthread_mutex_init(lock0.mutex, &att));

	char tmpfile[PATH_MAX];
	saprintf(tmpfile, "%s.%d.tmp", ln->name, getpid());

	// Ensure location is clear
	(void)unlink(tmpfile);
	errno = 0;

	/* Make the file */
	fd = open(tmpfile, O_RDWR|O_CREAT|O_CLOEXEC|O_TRUNC|O_EXCL, 0600);
	if(fd < 0) {
	    // Directory missing...
	    if (errno == ENOENT) { perror(ln->name); return 0; }
	    // Shouldn't happen, but wait and retry.
	    msleep(20);
	    continue;
	}

	int doclose = 1;

	// Write the mutex to the file
	int rv = write(fd, &lock0, sizeof(lock0));
	rv = -(rv != sizeof(lock0));
	if (rv == 0)
	    rv = close(fd);
	if (rv == 0)
	    doclose = 0;
	if (rv == 0)
	    rv = link(tmpfile, ln->name);

	if (doclose) (void)close(fd);
	(void)unlink(tmpfile);

	if (rv < 0)
	    msleep(100);

	/* We go round again to actually map it. */
    }
}

void
lock_stop(filelock_t * ln)
{
    if (!ln->lock) return;
    if (ln->have_lock)
	unlock_fn(ln);
   (void)munmap(ln->lock, getpagesize());
   ln->lock = 0;
}

void
lock_restart(filelock_t * ln)
{
    if (ln->lock) {
	if (ln->have_lock)
	    unlock_fn(ln);
	lock_stop(ln);
    }
    lock_start(ln);
}
#endif
#endif

#ifdef TEST
#ifndef _REENTRANT
#include <stdio.h>
#include "fcntl_mutex.c"
#endif
#include <sys/wait.h>

// This runs multiple processes on a lockfile.
// All should complete including No.5 (which should drop out early)
// The final value in /tmp/datafile should be the number of runs.

#define RUNS 20
int
main(int argc, char **argv)
{
#ifndef _REENTRANT
    fprintf(stderr, "Note: using fcntl_mutex.c\n");
#endif

    char * filename = argc>1&&argv[1][0]?argv[1]:"/tmp/mutex";
    char * fn2 = argc>2&&argv[2][0]?argv[2]:"/tmp/datafile";
    int pc = 0, pcount = 0;
    int pid[RUNS];
    int exit_pid = 0, st;

    // Create a lock file and try to break it.
    if (fork() == 0) {
	filelock_t llock[1] = {{0}};
	llock->name = filename;
	fprintf(stderr, "Start lockfile\n");
	lock_start(llock);
	fprintf(stderr, "Lock lockfile\n");
	lock_fn(llock);
	fprintf(stderr, "Exit process without unlocking\n");
	exit(0);
    } else
	exit_pid = wait(&st);

    if (exit_pid < 0 || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
	fprintf(stderr, "Subprocess exited unexpectedly\n");
	exit(1);
    }

    // Does the the lock file still work ?
    if (fork() == 0) {
	filelock_t llock[1] = {{0}};
	llock->name = filename;
	fprintf(stderr, "Start lockfile\n");
	lock_start(llock);
	fprintf(stderr, "Lock lockfile\n");
	lock_fn(llock);
	// Clear counter file.
	unlink(fn2);
	fprintf(stderr, "Unlock lockfile\n");
	unlock_fn(llock);
	exit(0);
    } else
	exit_pid = wait(&st);

    if (exit_pid < 0 || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
	fprintf(stderr, "Subprocess exited unexpectedly\n");
	exit(1);
    }

    // Does the lock file actually lock..
    for(pc=0; pc<RUNS; pc++) {
	pid[pc] = fork();
	if (pid[pc]) {
	    if (pid[pc] > 0)
		pcount++;
	    continue;
	}

	{
	    filelock_t llock[1] = {{0}};
	    llock->name = filename;
	    lock_start(llock);

	    lock_fn(llock);

	    FILE * fd = fopen(fn2, "r");
	    int v = 0;
	    if (fd) {
		msleep(10);
		fscanf(fd, "%d", &v);
		fclose(fd);
	    }
	    msleep(10);

	    fd = fopen(fn2, "w");
	    if (!fd) {
		perror("fopen");
		exit(1);
	    }
	    msleep(10);
	    fprintf(fd, "%d\n", v+1);
	    msleep(10);
	    fclose(fd);
	    msleep(10);

	    if (v == 5) exit(5);
	    unlock_fn(llock);

	    exit(0);
	}
    }

    fprintf(stderr, "Processes started: %d\n", pcount);

    while((exit_pid = wait(&st)) > 0) {
	fprintf(stderr, "Process %d exit -> 0x%04x\n", exit_pid, st);
    }

    {
	FILE * fd = fopen(fn2, "r");
	int v = 0;
	if (fd) {
	    msleep(10);
	    fscanf(fd, "%d", &v);
	    fclose(fd);
	}

#ifndef _REENTRANT
	fprintf(stderr, "FCNTL Result = %d%s\n", v, v==RUNS?" -> good":"\033[5;31;40m FAILED \033[m");
#else
	fprintf(stderr, "Result = %d%s\n", v, v==RUNS?" -> good":"\033[5;31;40m FAILED \033[m");
#endif
    }
}
#endif
