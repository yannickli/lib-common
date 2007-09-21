##########################################################################
#                                                                        #
#  Copyright (C) 2004-2007 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

DEPTH=..
include $(DEPTH)/admin/header.mk

__EXE__   = btree-dump
__LIB__   = libcommon
__CHECK__ = test tst-blob-iconv
__CHECK__ += tst-iprintf tst-iprintf-fp tst-iprintf-glibc tst-hash tst-sort
__CHECK__ += tst-iprintf-speed
# These tests are just too long to be part of "make check". Sorry.
__CHECK__ += tst-btree tst-isndx

btree-dump_SOURCES = btree-dump.c libcommon.a

libcommon_SOURCES = \
	archive.c \
	array.c \
	bfield.c \
	btree.c \
	concatbin.c  \
	conf.c  \
	dtoa.c \
	err_debug.c   \
	err_report.c  \
	farch.c \
	fifo.c \
	int_array.c \
	ioveclist.c \
	iprintf.c \
	isndx.c \
	licence.c \
	list.c \
	log_file.c \
	log_limit.c \
	mmappedfile.c \
	paged-index.c \
	property.c \
	robuf.c \
	showflags.c \
	stats-temporal.c \
	stopper.c \
	str_array.c \
	strconv.c \
	string_is.c \
	timeval.c \
	xml.c \
	\
	blob.c \
	blob-ebcdic.c \
	blob-utf8.c \
	blob-zlib.c \
	blob-iconv.c \
	blob-emi.c \
	\
	hash-crc.c \
	hash-sha2.c \
	hash-sha1.c \
	\
	mem.c \
	mem-pool.c \
	mem-fifo-pool.c \
	\
	unix.c \
	unix-linux.c \
	\
	$(end_of_list)

test_SOURCES = $(libcommon_SOURCES) check.c
test_LDFLAGS = -lz

tst-blob-iconv_SOURCES = \
	tst-blob-iconv.c \
	libcommon.a

tst-iprintf_SOURCES = tst-iprintf.c libcommon.a

tst-iprintf-fp_CFLAGS = -Wno-format -Wno-missing-format-attribute
tst-iprintf-fp_SOURCES = tst-iprintf-fp.c libcommon.a

tst-iprintf-glibc_CFLAGS = -Wno-format -Wno-missing-format-attribute
tst-iprintf-glibc_SOURCES = tst-iprintf-glibc.c libcommon.a

tst-hash_SOURCES = tst-hash.c libcommon.a

tst-btree_SOURCES = tst-btree.c btree.c libcommon.a
tst-isndx_SOURCES = tst-isndx.c libcommon.a
tst-sort_SOURCES = tst-sort.c libcommon.a
tst-sort_CFLAGS = -UCHECK
tst-sort_LDFLAGS = -lm
tst-iprintf-speed_SOURCES = tst-iprintf-speed.c libcommon.a
tst-iprintf-speed_CFLAGS = -UCHECK

ifneq (,$(filter CYGWIN%,$(UNAME)))
  libcommon_SOURCES := $(filter-out blob-iconv.c linux.c,$(libcommon_SOURCES))
  __CHECK__ := $(filter-out tst-blob-iconv,$(__CHECK__))
endif

ifneq (,$(MINGCC))
  # Disable some stuff that does not compile under MingW
  libcommon_SOURCES := $(filter-out unix-linux.c mmappedfile.c btree.c stats-temporal.c, $(libcommon_SOURCES))
  __EXE__ := $(filter-out btree-dump, $(__EXE__))
endif

include $(DEPTH)/admin/footer.mk
