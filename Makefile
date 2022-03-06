
CFLAGS=-O2 -Wall -Wextra -Wno-sign-compare -Wno-pointer-sign ${DEFS}
LDFLAGS=-lz
#TARGET_ARCH=-m64
PROG=server
SRC:=$(filter-out lib_%.c,$(wildcard *.c) )
NIL:=$(shell makeheaders -H >md5.h lib_md5.c)
NIL:=$(shell makeheaders ${SRC} md5.h)
NIL:=$(shell makeheaders lib_md5.c)
OBJ:=$(patsubst %.c,%.o,${SRC}) lib_md5.o

${PROG}: ${OBJ}
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -o ${PROG} ${OBJ} $(LDFLAGS)

include $(shell echo ${OBJ} | tr ' ' '\012' | sed 's/\(.*\)\.o/\1.o: \1.c \1.h/' > tmp.mk ; echo tmp.mk )

clean:
	-rm -f ${PROG} tmp.mk *.o $(patsubst %.c,%.h,$(wildcard *.c)) md5.h

