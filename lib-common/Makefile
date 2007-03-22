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
__CHECK__ = test test_blob_iconv msg_test 
__CHECK__ += tst-iprintf tst-iprintf-fp tst-iprintf-glibc tst-sha2
# These tests are just too long to be part of "make check". Sorry.
#__CHECK__ += tst-btree tst-isndx

btree-dump_SOURCES = btree-dump.c libcommon.a

libcommon_SOURCES = \
	archive.c \
	array.c \
	bfield.c \
	btree.c \
	isndx.c \
	str_array.c \
	blob.c \
	blob_ebcdic.c \
	blob_zlib.c \
	blob_iconv.c \
	blob_emi.c \
	concatbin.c  \
	conf.c  \
	err_report.c  \
	err_debug.c   \
	fifo.c \
	ioveclist.c \
	log_file.c \
	mmappedfile.c \
	paged-index.c \
	string_is.c \
	iprintf.c \
	strconv.c \
	timeval.c \
	msg_template.c \
	stopper.c \
	unix.c \
	mem-pool.c \
	mem-fifo-pool.c \
	sha2.c \
	xml.c \
	licence.c

# FIXME showflags should not be compiled in if EXPIRATION_DATE is undefined
# OG: why not ?
libcommon_SOURCES += showflags.c

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

msg_test_CFLAGS = -DMSG_TEST
msg_test_SOURCES = msg_template.c libcommon.a

ifneq (,$(filter CYGWIN%,$(UNAME)))
  libcommon_SOURCES := $(filter-out blob_iconv.c,$(libcommon_SOURCES))
  __CHECK__ := $(filter-out test_blob_iconv,$(__CHECK__))
endif

include $(DEPTH)/admin/footer.mk
