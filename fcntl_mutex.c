
#include <fcntl.h>

#include "fcntl_mutex.h"

#if !defined(HAS_PTHREAD)

#if INTERFACE
#define filelock_t fcntl_filelock_t
typedef struct fcntl_filelock_t fcntl_filelock_t;
struct fcntl_filelock_t {
    char * name;
    int fcntl_fd;
};
#endif

void lock_fn(fcntl_filelock_t * ln)
{
    if (!ln->fcntl_fd) return;
    share_lock(ln->fcntl_fd, F_SETLKW, F_WRLCK);
}

void unlock_fn(fcntl_filelock_t * ln)
{
    if (!ln->fcntl_fd) return;
    share_lock(ln->fcntl_fd, F_SETLK, F_UNLCK);
}

void lock_start(fcntl_filelock_t * ln)
{
    int fd = open(ln->name, O_RDWR|O_CLOEXEC|O_CREAT, 0600);
    if (fd <= 0) {
	perror(ln->name);
	exit(1);
    }
    ln->fcntl_fd = fd;
}

void lock_stop(fcntl_filelock_t * ln)
{
    if (!ln->fcntl_fd) return;
    close(ln->fcntl_fd);
    ln->fcntl_fd = 0;
}

void lock_restart(fcntl_filelock_t * ln)
{
    lock_stop(ln);
    lock_start(ln);
}

LOCAL void
share_lock(int fd, int mode, int l_type)
{
   for(int ec=0;;ec++)
   {
      int rv;
      struct flock lck = {0};
      lck.l_type = l_type;
      lck.l_whence = SEEK_SET;
      lck.l_start = 0;
      lck.l_len = 0;

      rv = fcntl(fd, mode, &lck);
      if (rv >= 0) return;
      if (rv < 0 && (ec > 20 || (errno != EINTR))) {
	 perror("Problem locking shared area");
	 return;
      }
      // NB: Kernel detects deadlocks so this can be bad.
      msleep(50);
   }
}
#endif
