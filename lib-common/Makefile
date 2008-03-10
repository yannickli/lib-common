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

DEPTH=..
include $(DEPTH)/admin/header.mk
include $(DEPTH)/admin/tokyocabinet.mk

__EXE__   = btree-dump tst-cfgparser tst-tpl
__LIB__   = libcommon time-lp-simple
__CHECK__ = test tst-blob-iconv tst-domains_list
__CHECK__ += tst-iprintf tst-iprintf-fp tst-iprintf-glibc tst-hash
__CHECK__ += tst-iprintf-speed tst-hashtbl
ifdef CHECK_ALL
  # These tests are just too long to be part of "make check". Sorry.
  __CHECK__ += tst-btree tst-isndx tst-sort
  #__CHECK__ += tst-stats # currently broken
endif

btree-dump_SOURCES = btree-dump.c libcommon.a

libcommon_SOURCES = \
	archive.c \
	array.c \
	bfield.c \
	btree.c \
	concatbin.c  \
	db.c \
	dtoa.c \
	domains_list.c \
	err_debug.c   \
	err_report.c  \
	farch.c \
	fifo.c \
	hashtbl.c \
	iprintf.c \
	isndx.c \
	licence.c \
	list.c \
	log_file.c \
	log_limit.c \
	mmappedfile.c \
	paged-index.c \
	parseopt.c \
	psinfo.c \
	range-vector.c \
	robuf.c \
	showflags.c \
	stats-temporal.c \
	stopper.c \
	strconv.c \
	string_is.c \
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
	\
	conf.c  \
	conf-parser.l \
	\
	file-binlog.c \
	\
	hash-crc.c \
	hash-sha2.c \
	hash-sha1.c \
	hash-md5.c \
	\
	mem.c \
	mem-pool.c \
	mem-fifo-pool.c \
	\
	property.c \
	property-hash.c \
	\
	tpl.c \
	tpl-funcs.c \
	\
	unix.c \
	unix-linux.c \
	\
	$(end_of_list)
libcommon_CFLAGS := $(tokyocabinet_CFLAGS)

time-lp-simple_SOURCES = time-lp-simple.c

test_SOURCES = $(filter-out mem.c, $(libcommon_SOURCES)) check.c
test_CFLAGS  = $(tokyocabinet_CFLAGS)
test_LDFLAGS = -lz $(tokyocabinet_LDFLAGS)

tst-cfgparser_SOURCES = tst-cfgparser.c libcommon.a

tst-tpl_SOURCES = tst-tpl.c libcommon.a

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

tst-stats_SOURCES = tst-stats.c libcommon.a
tst-hashtbl_SOURCES = err_debug.c err_report.c tst-hashtbl.c

tst-domains_list_SOURCES = domains_list.c libcommon.a
tst-domains_list_CFLAGS = -DSTANDALONE

ifneq (,$(filter CYGWIN%,$(UNAME)))
  libcommon_SOURCES := $(filter-out blob-iconv.c linux.c psinfo.c,$(libcommon_SOURCES))
  __CHECK__ := $(filter-out tst-blob-iconv,$(__CHECK__))
endif

ifneq (,$(MINGCC))
  # Disable some stuff that does not compile under MingW
  libcommon_SOURCES := $(filter-out unix-linux.c mmappedfile.c psinfo.c btree.c stats-temporal.c, $(libcommon_SOURCES))
  __EXE__ := $(filter-out btree-dump, $(__EXE__))
endif

include $(DEPTH)/admin/footer.mk
