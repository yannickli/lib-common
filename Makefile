##########################################################################
#                                                                        #
#  Copyright (C) 2004-2008 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

test_PROGRAMS = btree-dump tst-cfgparser tst-tpl tst-htbl

none_LIBRARIES = libcommon time-lp-simple

none_TESTS += test tst-blob-iconv tst-path tst-hash
none_TESTS += tst-iprintf tst-iprintf-fp tst-iprintf-glibc tst-iprintf-speed
ifdef CHECK_ALL
  # These tests are just too long to be part of "make check". Sorry.
  none_TESTS += tst-btree tst-isndx tst-sort
else
  test_PROGRAMS += tst-btree tst-isndx tst-sort
endif

btree-dump_SOURCES = btree-dump.c libcommon.a compat/compat.a
btree-dump_LDFLAGS = -lm

libcommon_SOURCES = \
	bfield.c \
	btree.c \
	farch.c \
	isndx.c \
	licence.c \
	mmappedfile.c \
	paged-index.c \
	parseopt.c \
	psinfo.c \
	showflags.c \
	stopper.c \
	time.c \
	xml.c \
	xmlpp.c \
	\
	blob.c \
	blob-ebcdic.c \
	blob-utf8.c \
	blob-zlib.c \
	blob-iconv.c \
	blob-emi.c \
	blob-wbxml.c \
	\
	conf.c  \
	conf-parser.l \
	\
	container-array.c \
	container-htbl.c \
	container-list.c \
	container-ring.c \
	\
	core-debug.c   \
	core-errors.c  \
	\
	file.c \
	file-log.c \
	\
	hash-crc.c \
	hash-hash.c \
	hash-sha2.c \
	hash-sha1.c \
	hash-md5.c \
	\
	iprintf.c \
	iprintf-dtoa.c \
	\
	mem-pool.c \
	mem-fifo-pool.c \
	\
	net-socket.c \
	\
	property.c \
	property-hash.c \
	\
	str.c \
	str-conv.c \
	str-path.c \
	\
	tpl.c \
	tpl-funcs.c \
	\
	unix.c \
	unix-linux.c \
	unix-solaris.c \
	\
	$(end_of_list)

time-lp-simple_SOURCES = time-lp-simple.c

test_SOURCES = $(libcommon_SOURCES) check.c $/lib-common/compat/check.c
test_CFLAGS  = -DCHECK=1
test_LDFLAGS = -lz

tst-cfgparser_SOURCES = tst-cfgparser.c libcommon.a compat/compat.a

tst-tpl_SOURCES = tst-tpl.c libcommon.a

tst-blob-iconv_SOURCES = \
	tst-blob-iconv.c \
	libcommon.a

tst-iprintf_SOURCES = tst-iprintf.c libcommon.a compat/compat.a

tst-iprintf-fp_CFLAGS = -Wno-format -Wno-missing-format-attribute
tst-iprintf-fp_SOURCES = tst-iprintf-fp.c libcommon.a

tst-iprintf-glibc_CFLAGS = -Wno-format -Wno-missing-format-attribute
tst-iprintf-glibc_SOURCES = tst-iprintf-glibc.c libcommon.a

tst-hash_SOURCES = tst-hash.c libcommon.a compat/compat.a

tst-btree_SOURCES = tst-btree.c btree.c libcommon.a compat/compat.a
tst-isndx_SOURCES = tst-isndx.c libcommon.a compat/compat.a
tst-sort_SOURCES = tst-sort.c libcommon.a compat/compat.a
tst-sort_CFLAGS = -UCHECK
tst-sort_LDFLAGS = -lm
tst-iprintf-speed_SOURCES = tst-iprintf-speed.c libcommon.a compat/compat.a
tst-iprintf-speed_CFLAGS = -UCHECK

tst-htbl_SOURCES = tst-htbl.c libcommon.a compat/compat.a
tst-path_SOURCES = tst-path.c libcommon.a compat/compat.a

ifneq (,$(filter CYGWIN%,$(UNAME)))
  libcommon_SOURCES := $(filter-out blob-iconv.c linux.c psinfo.c,$(libcommon_SOURCES))
  none_TESTS := $(filter-out tst-blob-iconv,$(none_TESTS))
endif

ifneq (,$(MINGCC))
  # Disable some stuff that does not compile under MingW
  libcommon_SOURCES := $(filter-out unix-linux.c mmappedfile.c psinfo.c btree.c, $(libcommon_SOURCES))
  test_PROGRAMS := $(filter-out btree-dump, $(test_PROGRAMS))
endif

ifneq (SunOS,$(shell uname -s))
DISTCLEANFILES=Upgrading.html
all:: Upgrading.html
Upgrading.html: Upgrading.txt ../Config/asciidoc.conf
	@echo " DOC $(@F)"
	@$(RM) $@
	@asciidoc -f ../Config/asciidoc.conf -b xhtml11 -o $@ $<
	@dos2unix $@
endif

include ../Build/base.mk
