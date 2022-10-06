/*
 * Define some includes for all files; I get makeheaders to include these
 * in every file by placing them before no-op definitions of some basic
 * types. This way any prototype seems to require these definitions.
 *
 * The #define "NEVER_DEFINE" shouldn't be defined; it's probably safe to tho.
 */
#if INTERFACE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>

#ifdef NEVER_DEFINE
#define int int
#define char char
#define void void
#define uint8_t uint8_t
#define uint16_t uint16_t
#define uint32_t uint32_t
#define uint64_t uint64_t
#define tm tm
#endif
#endif
