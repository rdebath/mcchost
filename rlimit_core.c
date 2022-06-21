#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "rlimit_core.h"

#if INTERFACE
#define HAS_CORELIMIT
#endif

void
enable_coredump()
{
    struct rlimit rlim[1];

    if (getrlimit(RLIMIT_CORE, rlim) < 0) {
	perror("getrlimit(RLIMIT_CORE)");
	return;
    }

    rlim->rlim_cur = RLIM_INFINITY;

    if (setrlimit(RLIMIT_CORE, rlim) < 0)
	perror("setrlimit(RLIMIT_CORE, rlim_cur=RLIM_INFINITY)");
}
