
#include "gcc.h"

#if INTERFACE

#if defined(__GNUC__) \
    && (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)

#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x

#ifndef __attribute__
#define __attribute__(__ignored__)
#endif
#endif

#define STFU(_x_) (_x_)

#ifndef __has_include
#define __has_include(_x) 0
#endif
#if __has_include(<stdatomic.h>)
#include <stdatomic.h>
#define STDATOMIC
#endif

#endif /*INTERFACE*/
