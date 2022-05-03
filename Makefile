
ifneq ($(MAKECMDGOALS),clean)
ifeq ($(findstring makeheaders,$(MAKECMDGOALS))$(findstring lib_text,$(MAKECMDGOALS)),)
NIL:=$(shell make lib_text >&2 )
NIL:=$(shell make makeheaders >&2 )
endif
endif

DEFS=-O2 -g3 -D_FILE_OFFSET_BITS=64
CFLAGS=-Wall -Wextra -Wno-sign-compare -Wno-pointer-sign ${DEFS}
LDFLAGS=-lz
#LDFLAGS=-Wl,-Bstatic -lz -Wl,-Bdynamic
#TARGET_ARCH=-m64
PROG=server
SRC:=$(filter-out lib_%.c,$(wildcard *.c) )
OBJ:=$(patsubst %.c,%.o,${SRC}) lib_md5.o
OBJ2=lib_text.o
INAME=mcchost-server

${PROG}: ${OBJ} ${OBJ2}
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -o ${PROG} ${OBJ} ${OBJ2} $(LDFLAGS)

.PHONY: install clean makeheaders lib_text

install: ${PROG}
	cp -p ${PROG} "${HOME}/bin/${INAME}"

ifeq ($(MAKECMDGOALS),clean)
clean:
	-rm -f ${PROG} tmp.mk *.o $(patsubst %.c,%.h,$(wildcard *.c)) md5.h lib_text.c
	make -C lib clean
else
ifneq ($(findstring clean,$(MAKECMDGOALS)),)
clean:
	@echo "make: Don't mix build with clean" >&2 ; false
endif
endif

ifneq ($(MAKECMDGOALS),clean)
include $(shell echo ${OBJ} | tr ' ' '\012' | sed 's/\(.*\)\.o/\1.o: \1.c \1.h/' > tmp.mk ; echo tmp.mk )
endif

makeheaders:
	make -C lib
	lib/makeheaders lib_md5.c
	lib/makeheaders -H >md5.h lib_md5.c
	lib/makeheaders ${SRC} lib_text.c md5.h

lib_text:
	awk -f help_scan.awk *.c > tmp.c
	cmp -s tmp.c lib_text.c || mv tmp.c lib_text.c
	-@rm -f tmp.c

lib_text.o: lib_text.c

export TARGET_ARCH DEFS LDFLAGS
vps:
	$(MAKE) clean
	$(MAKE) -j
	rsync -Pax ${PROG} vps-mcc:bin/mcchost-server
