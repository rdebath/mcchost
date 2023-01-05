
#include "typemagic.h"

#if INTERFACE
typedef char * charp;
struct magic_int { int a; char b; };
struct magic_long { long a; char b; };
struct magic_charp { char *a; char b; };
struct magic_double { double a; char b; };
struct magic_time_t { time_t a; char b; };
struct magic_int16_t { int16_t a; char b; };
struct magic_int64_t { int64_t a; char b; };

#define MAGICPART(XTYPE,STYPE) \
    (((sizeof(XTYPE)-1)<<3)+(sizeof(struct magic_##XTYPE)-sizeof(XTYPE)-1))

#define TY_MAGIC \
    (MAGICPART(double, magic_double) +  \
    (MAGICPART(int64_t, magic_int64_t) << 6) +  \
    (MAGICPART(time_t, magic_time_t) << 12) +  \
    (MAGICPART(charp, magic_charp) << 18) +  \
    (MAGICPART(long, magic_long) << 24) +  \
    ((int64_t)MAGICPART(int16_t, magic_int16_t) << 30) + \
    ((int64_t)MAGICPART(int, magic_int) << 36) + \
    ((int64_t)0x1A7F << 48))

#endif

#if TEST
#include <stdio.h>
void main() {
    printf("       0 I H L P T 6 D\n", (intmax_t)EVILMAGIC);
    printf("%022jo\n", (intmax_t)EVILMAGIC);

}
#endif
