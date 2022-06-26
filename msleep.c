
#include <unistd.h>
#include <time.h>
#include <errno.h>

#if _POSIX_VERSION >= 200112L

void
msleep(long millisec)
{
    struct timespec req = {0};
    time_t sec = millisec / 1000;
    millisec -= (sec * 1000L);
    req.tv_sec = sec;
    req.tv_nsec = millisec * 1000000L;
    while(nanosleep(&req, &req) == -1 && errno == EINTR) ;
}

#else

void
msleep(long millisec)
{
    usleep(millisec * 1000L);
}

#endif
