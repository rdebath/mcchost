
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "fcntl_mutex.h"

#if 0

#if INTERFACE
typedef struct filelock_t filelock_t;
struct filelock_t {
    char * name;
    int have_lock;
    int fnctl_fd;
};
#endif

void lock_fn(filelock_t * ln);
void unlock_fn(filelock_t * ln);
void lock_start(filelock_t * ln);
void lock_stop(filelock_t * ln);

//TODO redo this with lock_start and lock_fn

void
lock_shared(int fcntl_fd)
{
   share_lock(fcntl_fd, F_SETLKW, F_WRLCK);
}

void
unlock_shared(int fcntl_fd)
{
   share_lock(fcntl_fd, F_SETLK, F_UNLCK);
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

#endif
