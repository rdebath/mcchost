PROG=server
ODIR=obj

# Always ensure headers are up to date, unless we're cleaning.
ifeq ($(findstring zip,$(MAKECMDGOALS)),)
ifeq ($(findstring clean,$(MAKECMDGOALS)),)
ifeq ($(findstring rebuild,$(MAKECMDGOALS)),)
ifeq ($(findstring makeheaders,$(MAKECMDGOALS)),)
NIL:=$(shell $(MAKE) $(MFLAGS) ODIR="${ODIR}" makeheaders >&2 )
NIL:=
endif
endif
endif
endif

DEFS=
# Move the source for install
DBGSRC=-fwrapv -fdebug-prefix-map='$(shell pwd)'=src
# Use -D_FILE_OFFSET_BITS=64 to allow larger maps with a 32bit compile
# WTH is the point of the "truncation" warnings!
WARN=-Wall -Wextra -Wno-sign-compare -Wno-pointer-sign -Wno-format-truncation -Wno-stringop-truncation
# For Clang? -Wno-unknown-warning-option
CFLAGS=-Iinclude -O2 -g3 ${PTHREAD} ${LIBLMDBDEF} ${WARN} ${DEFS} ${HEADER} ${DBGSRC}
PTHREAD=-pthread
LIBLMDB=-llmdb

# May be needed for older systems (eg: Debian Etch on x86)
# CC=c99 -D_GNU_SOURCE
# Also: LIBLMDB=

# Pick one for cross compile, use ODIR and PROG for out of tree binaries.
#TARGET_ARCH=-m64
#TARGET_ARCH=-mx32
#TARGET_ARCH=-m32

ifeq ($(LIBLMDB),)
LIBLMDBDEF=-DDISABLE_LMDB
endif
LDFLAGS=-lz -lm ${LIBLMDB}
HEADER=$(if $(findstring .c,$<),-DHEADERFILE='"$(patsubst %.c,%.h,$<)"')

SRC=$(wildcard *.c command/*.c)
OBJ:=$(patsubst %.c,${ODIR}/%.o,$(shell echo ${SRC} | sed 's@[^ ]*/@@g'))
OBJADD=${ODIR}/lib_text.o ${ODIR}/lib_md5.o

# We like a longer name so the command line is bigger for our argv mangling.
INAME=mcchost-server
INSTDIR=${HOME}/bin
# Use rsync so the executable isn't touched when it's the same bytes.
INSTALLER=rsync -Pax

${PROG}: ${OBJ} ${OBJADD}
ifeq ($(findstring s,$(MFLAGS)),)
	@echo "$(CC) \$${CFLAGS} -o ${PROG} \$${OBJLIST} \$${LDFLAGS}"
endif
	@$(CC) -o ${PROG} $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) ${OBJ} ${OBJADD} $(LDFLAGS)

.PHONY: install clean makeheaders zip

install: ${PROG}
	${INSTALLER} ${PROG} "${INSTDIR}/${INAME}"

ifeq ($(MAKECMDGOALS),clean)
clean:
	-rm -f ${PROG} lib/makeheaders
	-rm -rf ${ODIR} include
	-rm -f lib/lib_text.c ${ODIR}/build-opts.* tmp.mk
else
ifneq ($(findstring clean,$(MAKECMDGOALS)),)
clean:
	@echo "Makefile: Don't mix build with clean, did you want 'make rebuild' ..." >&2
	-@sed -n '/^rebuild/,/^$$/p' Makefile
	@false
endif
endif

ifneq ($(MAKECMDGOALS),zip)
ifneq ($(MAKECMDGOALS),clean)
include $(shell \
    echo ${SRC} | tr ' ' '\012' |\
    sed 's@\(.*/\)\{0,1\}\([^\/ ]*\)\.c@${ODIR}/\2.o: \1\2.c include/\2.h@' > tmp.mk ;\
    echo tmp.mk )
endif
endif

MKHDRARG:=$(shell echo ${SRC} lib/lib_text.c |tr ' ' '\012' | sed 's@\(.*/\)\{0,1\}\([^\/ ]*\)\.c@\1\2.c:include/\2.h@' )

makeheaders: ${ODIR}/makeheaders
	-@rm -f lib_text.c
	@echo "awk -f help_scan.awk \$${SRC} > tmp.h"
	@awk -f help_scan.awk ${SRC} > tmp.h
	cmp -s tmp.h lib/lib_text.c || mv tmp.h lib/lib_text.c
	-@rm -f tmp.h
	@:
	@mkdir -p include
	@sh version.sh include/version.h
	${ODIR}/makeheaders lib/lib_md5.c:include/lib_md5.h
	${ODIR}/makeheaders -H >include/md5.h lib/lib_md5.c
ifeq ($(findstring s,$(MFLAGS)),)
	@echo "${ODIR}/makeheaders \$${FILES} include/md5.h include/version.h"
endif
	@${ODIR}/makeheaders ${MKHDRARG} include/md5.h include/version.h

${ODIR}/makeheaders: lib/makeheaders.c
	@mkdir -p ${ODIR}
	$(CC) -O -o $@ $<

${ODIR}/lib_text.o: lib/lib_text.c include/lib_text.h
${ODIR}/lib_md5.o: lib/lib_md5.c include/lib_md5.h

.SUFFIXES:

ifeq ($(findstring s,$(MFLAGS)),)
define comp_c
	@[ "$V" = '' ] && echo "$(CC) \$${CFLAGS} -c -o" $@ $< ||:
	@[ "$V" != '' ] && echo "$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<" ||:
	@$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<
endef
else
define comp_c
	@$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<
endef
endif

${ODIR}/%.o: %.c
	$(comp_c)

${ODIR}/%.o: command/%.c
	$(comp_c)

${ODIR}/%.o: lib/%.c
	$(comp_c)

export TARGET_ARCH DEFS LDFLAGS

BUILDOPT=$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(TARGET_ARCH)
BUILDFLG :=${ODIR}/build-opts.$(shell echo '$(BUILDOPT)' | sum | tr -d '\040\055' )
$(PROG) ${OBJ} ${OBJADD}: $(BUILDFLG)
$(BUILDFLG):
	@mkdir -p ${ODIR} include
	-@rm -f ${ODIR}/build-opts.*
	@echo '$(BUILDOPT)' > $(BUILDFLG)

rebuild:
	$(MAKE) clean
	$(MAKE) $(PROG) -j$$(nproc)

#
# Make zip and tgz files with full test compile in i86 and x64.
# Note: This needs libraries for BOTH 64 bit and 32 bit.
#
ZIPF1=LICENSE LICENSE.mcchost GNUmakefile
ZIPF2=Makefile help_scan.awk version.sh include/version.h
ZIPF3=lib/Readme.txt lib/makeheaders.c lib/makeheaders.html lib/lib_md5.c
ZIPF=${ZIPF1} ${SRC} ${ZIPF2} ${ZIPF3}

zip:
	-@rm -rf tmp.tgz tmp.d
	@:
	@echo 'tar cf tmp.tgz -I "gzip -9" $${ZIPF}'
	@tar cf tmp.tgz -I "gzip -9" ${ZIPF}
	@mkdir -p tmp.d
	cd tmp.d ; tar xf ../tmp.tgz
	cd tmp.d ; make -j$$(nproc) -s TARGET_ARCH=-m64 ODIR=obj64 PROG=obj64/server
	-@rm -rf tmp.d
	@:
	@echo 'zip -9q tmp.zip $${ZIPF}'
	@zip -9q tmp.zip ${ZIPF}
	@mkdir -p tmp.d
	cd tmp.d ; unzip -q ../tmp.zip
	cd tmp.d ; make -j$$(nproc) -s TARGET_ARCH=-m32 ODIR=obj32 PROG=obj32/server
	-@rm -rf tmp.d
	@:
	@mkdir -p zip
	@echo \* > zip/.gitignore
	mv tmp.tgz zip/mcchost.tgz
	mv tmp.zip zip/mcchost.zip
