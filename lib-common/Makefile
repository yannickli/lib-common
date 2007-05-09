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
__CHECK__ = test test_blob_iconv
__CHECK__ += tst-iprintf tst-iprintf-fp tst-iprintf-glibc tst-sha2 tst-sort
__CHECK__ += tst-iprintf-speed
# These tests are just too long to be part of "make check". Sorry.
#__CHECK__ += tst-btree tst-isndx

btree-dump_SOURCES = btree-dump.c libcommon.a

libcommon_SOURCES = \
	archive.c \
	array.c \
	bfield.c \
	btree.c \
	compat.c \
	isndx.c \
	str_array.c \
	blob.c \
	blob_ebcdic.c \
	blob_utf8.c \
	blob_zlib.c \
	blob_iconv.c \
	blob_emi.c \
	concatbin.c  \
	conf.c  \
	err_report.c  \
	err_debug.c   \
	fifo.c \
	ioveclist.c \
	list.c \
	log_file.c \
	log_limit.c \
	mmappedfile.c \
	paged-index.c \
	string_is.c \
	iprintf.c \
	dtoa.c \
	strconv.c \
	timeval.c \
	showflags.c \
	stopper.c \
	linux.c \
	unix.c \
	mem-pool.c \
	mem-fifo-pool.c \
	sha2.c \
	xml.c \
	licence.c

test_SOURCES = $(libcommon_SOURCES) check.c
test_LDFLAGS = -lz

test_blob_iconv_SOURCES = \
	test_blob_iconv.c \
	libcommon.a

tst-iprintf_SOURCES = tst-iprintf.c libcommon.a

tst-iprintf-fp_CFLAGS = -Wno-format -Wno-missing-format-attribute
tst-iprintf-fp_SOURCES = tst-iprintf-fp.c libcommon.a

tst-iprintf-glibc_CFLAGS = -Wno-format -Wno-missing-format-attribute
tst-iprintf-glibc_SOURCES = tst-iprintf-glibc.c libcommon.a

tst-sha2_CFLAGS = -DTEST_VECTORS
tst-sha2_SOURCES = sha2.c libcommon.a

tst-btree_SOURCES = tst-btree.c libcommon.a
tst-isndx_SOURCES = tst-isndx.c libcommon.a
tst-sort_SOURCES = tst-sort.c libcommon.a
tst-sort_CFLAGS = -UCHECK
tst-sort_LDFLAGS = -lm
tst-iprintf-speed_SOURCES = tst-iprintf-speed.c libcommon.a
tst-iprintf-speed_CFLAGS = -UCHECK

ifneq (,$(filter CYGWIN%,$(UNAME)))
  libcommon_SOURCES := $(filter-out blob_iconv.c linux.c,$(libcommon_SOURCES))
  __CHECK__ := $(filter-out test_blob_iconv,$(__CHECK__))
endif

ifneq (,$(MINGCC))
  # Disable some stuff that does not compile under MingW
  libcommon_SOURCES := $(filter-out linux.c, $(libcommon_SOURCES))
endif
# OG: should filter libcommon_SOURCES for MINGCC as well 
#  libcommon_SOURCES := $(filter-out licence.c iprintf.c ioveclist.c
#  blob_emi.c blob_iconv.c blob_zlib.c concatbin.c log_file.c btree.c
#  mem-fifo-pool.c mmappedfile.c paged-index.c,\
#  $(libcommon_SOURCES))
# __EXE__ := $(filter-out btree-dump, $(__EXE__))

include $(DEPTH)/admin/footer.mk
