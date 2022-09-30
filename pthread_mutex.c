
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "pthread_mutex.h"

#if INTERFACE
#include <pthread.h>

typedef struct filelock_t filelock_t;
struct filelock_t {
    char * name;
    int have_lock; // Not allowed to "nest" pthread locks, but can't
		   // portably configure PTHREAD_MUTEX_ERRORCHECK
    pthread_mutex_t * mutex;
};
#endif

#if _REENTRANT

// This MUST be compiled with -pthread otherwise gcc makes dumb assumptions.

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
    if (!ln->mutex || ln->have_lock) return;

    if (E(pthread_mutex_lock(ln->mutex)))
	E(pthread_mutex_consistent(ln->mutex));
    ln->have_lock = 1;
}

void
unlock_fn(filelock_t * ln)
{
    if (!ln->mutex || !ln->have_lock) return;
    E(pthread_mutex_unlock(ln->mutex));
    ln->have_lock = 0;
}

void
lock_start(filelock_t * ln)
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
    ln->mutex = 0;
    if (!ln->name) return;

    while(1)
    {
	if (errno != 0 && ecount > 100) { perror(ln->name); return; }

	fd = open(ln->name, O_RDWR|O_CLOEXEC);
	if (fd>=0) break;

	// ENOENT -> Need to make; other -> not good.
	if (errno != ENOENT || ecount > 100) { perror(ln->name); return; }
	ecount++;

	// Create the mutex in local memory
	pthread_mutexattr_t att;
	pthread_mutex_t mutex0 = {0};
	E(pthread_mutexattr_init(&att));
	E(pthread_mutexattr_setrobust(&att, PTHREAD_MUTEX_ROBUST));
	E(pthread_mutexattr_setpshared(&att, PTHREAD_PROCESS_SHARED));
	E(pthread_mutex_init(&mutex0, &att));

	char tmpfile[PATH_MAX];
	saprintf(tmpfile, "%s.%d.tmp", ln->name, getpid());

	// Ensure location is clear
	(void)unlink(tmpfile);
	errno = 0;

	/* Make the file */
	fd = open(tmpfile, O_RDWR|O_CREAT|O_CLOEXEC|O_TRUNC|O_EXCL, 0600);
	if(fd < 0) {
	    // Directory missing...
	    if (errno == ENOENT) { perror(ln->name); return; }
	    // Shouldn't happen, but wait and retry.
	    usleep(20000);
	    continue;
	}

	int doclose = 1;

	// Write the mutex to the file
	int rv = write(fd, &mutex0, sizeof(mutex0));
	rv = -(rv != sizeof(mutex0));
	if (rv == 0)
	    rv = close(fd);
	if (rv == 0)
	    doclose = 0;
	if (rv == 0)
	    rv = link(tmpfile, ln->name);

	if (doclose) (void)close(fd);
	(void)unlink(tmpfile);

	if (rv < 0)
	    usleep(100000);

	/* We go round again to actually map it. */
    }

    pthread_mutex_t * mutex;
    mutex = mmap(NULL, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    ln->mutex = mutex;
}

void
lock_stop(filelock_t * ln)
{
    if (!ln->mutex) return;
    if (ln->have_lock)
	unlock_fn(ln);
   (void)munmap(ln->mutex, getpagesize());
   ln->mutex = 0;
}

#else
void lock_fn(filelock_t * ln);
void unlock_fn(filelock_t * ln);
void lock_start(filelock_t * ln);
void lock_stop(filelock_t * ln);
#endif

#ifdef TEST
static filelock_t llock[1] = {{0}};

int
main(int argc, char **argv)
{
    char * filename = argc>1&&argv[1][0]?argv[1]:"/tmp/mutex";
    char * fn2 = argc>2&&argv[2][0]?argv[2]:"/tmp/datafile";

    llock->name = filename;
    lock_start(llock);

    lock_fn(llock);

    FILE * fd = fopen(fn2, "r");
    int v = 0;
    if (fd) {
	usleep(10000);
	fscanf(fd, "%d", &v);
	fclose(fd);
    }
	usleep(10000);

    fd = fopen(fn2, "w");
	usleep(10000);
    if (!fd) {
	perror("fopen");
	exit(1);
    }
    fprintf(fd, "%d\n", v+1);
	usleep(10000);
    fclose(fd);
	usleep(10000);

    unlock_fn(llock);
}
#endif
