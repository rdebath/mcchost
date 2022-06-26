# Always ensure headers are up to date, unless we're cleaning.
ifeq ($(findstring clean,$(MAKECMDGOALS)),)
ifeq ($(findstring rebuild,$(MAKECMDGOALS)),)
ifeq ($(findstring makeheaders,$(MAKECMDGOALS)),)
NIL:=$(shell make makeheaders >&2 )
NIL:=
endif
endif
endif

DEFS=
# Use -D_FILE_OFFSET_BITS=64 to allow larger maps with a 32bit compile
WARN=-Wall -Wextra -Wno-sign-compare -Wno-pointer-sign
CFLAGS=-O2 -g3 ${WARN} ${DEFS}
LDFLAGS=-lz -lm -llmdb
HEADER=$(if $(findstring .c,$<),-DHEADERFILE='"$(patsubst %.c,%.h,$<)"')

# May be needed for older systems (eg: Debian Etch on x86)
# CC=c99 -D_GNU_SOURCE

# Pick one for cross compile
#TARGET_ARCH=-m64
#TARGET_ARCH=-mx32
#TARGET_ARCH=-m32

PROG=server
SRC:=$(filter-out lib_%.c,$(wildcard *.c) )
OBJ:=$(patsubst %.c,%.o,${SRC}) lib_md5.o
OBJ2=lib_text.o

# We like a longer name so the command line is bigger for our argv mangling.
INAME=mcchost-server
INSTDIR=${HOME}/bin
INSTALLER=rsync -ax

${PROG}: ${OBJ} ${OBJ2}
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -o ${PROG} ${OBJ} ${OBJ2} $(LDFLAGS)

.PHONY: install clean makeheaders lib_text

install: ${PROG}
	${INSTALLER} ${PROG} "${INSTDIR}/${INAME}"

ifeq ($(MAKECMDGOALS),clean)
clean:
	-rm -f ${PROG} *.o $(patsubst %.c,%.h,$(wildcard *.c)) md5.h lib_text.c
	-rm -f lib/makeheaders
	-rm -f .build-opts.* tmp.mk
else
ifneq ($(findstring clean,$(MAKECMDGOALS)),)
clean:
	@echo "Makefile: Don't mix build with clean, did you want 'make rebuild' ..." >&2
	-@sed -n '/^rebuild/,/^$$/p' Makefile
	@false
endif
endif

ifneq ($(MAKECMDGOALS),clean)
include $(shell echo ${OBJ} | tr ' ' '\012' | sed 's/\(.*\)\.o/\1.o: \1.c \1.h/' > tmp.mk ; echo tmp.mk )
endif

makeheaders: lib/makeheaders
	awk -f help_scan.awk *.c > tmp.c
	cmp -s tmp.c lib_text.c || mv tmp.c lib_text.c
	-@rm -f tmp.c
	:
	lib/makeheaders lib_md5.c
	lib/makeheaders -H >md5.h lib_md5.c
	lib/makeheaders ${SRC} lib_text.c md5.h

lib/makeheaders: lib/makeheaders.c
	$(CC) -O2 -o $@ $<

lib_text.o: lib_text.c

ifeq ($(findstring s,$(MFLAGS)),)
%.o: %.c
	@echo "$(CC) [flags] -c -o" $@ $<
	@$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<
endif

export TARGET_ARCH DEFS LDFLAGS

BUILDOPT=$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(TARGET_ARCH)
BUILDFLG :=.build-opts.$(shell echo '$(BUILDOPT)' | sum | tr -d '\040\055' )
$(PROG) ${OBJ} ${OBJ2}: $(BUILDFLG)
$(BUILDFLG):
	-@rm -f .build-opts.*
	-@echo '$(BUILDOPT)' > $(BUILDFLG)

rebuild:
	make clean
	make $(PROG) -j$$(nproc)

vps: ${PROG}
	rsync -Pax ${PROG} vps-mcc:bin/mcchost-server
