
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

#if defined(__GNUC__) && __GNUC__ > 5 && !defined(__STRICT_ANSI__)
// How can I concisely tell this compiler that truncation is the whole damn point!
// Also as I have to do this, I've added a static assert so that it is known to
// be a large array not just a pointer
#define saprintf(array, ...) (void)(\
    ({enum{_x=1/(sizeof(array)-sizeof(char*))};}),\
    snprintf(array, sizeof(array), __VA_ARGS__)?({asm("");}):0)
#else
#define saprintf(array, ...) (void)snprintf(array, sizeof(array), __VA_ARGS__)
#endif

#endif /*INTERFACE*/
