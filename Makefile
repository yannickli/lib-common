##########################################################################
#                                                                        #
#  Copyright (C) INTERSEC SA                                             #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

test_PROGRAMS += zchk btree-dump ztst-cfgparser ztst-tpl ztst-htbl
test_PROGRAMS += ztst-lzo

none_LIBRARIES = libcommon time-lp-simple

none_TESTS += ztst ztst-path ztst-hash
none_PROGRAMS += ztst-iprintf ztst-iprintf-fp ztst-iprintf-glibc ztst-iprintf-speed
ifdef CHECK_ALL
  # These tests are just too long to be part of "make check". Sorry.
  none_TESTS += ztst-btree ztst-isndx ztst-sort
else
  test_PROGRAMS += ztst-btree ztst-isndx ztst-sort
endif

btree-dump_SOURCES = btree-dump.c libcommon.a compat/compat.a
btree-dump_LIBS = -lm

DISTCLEANFILES = core-version.c
core-version.c: scripts/version.sh FORCE
	$(msg/generate) $@
	$< rcsid libcommon > $@+
	$(call fun/update-if-changed,$@+,$@)

_LIBS = -lpthread

libcommon_SOURCES = \
	bfield.c \
	btree.c \
	farch.c \
	isndx.c \
	licence.c \
	mmappedfile.c \
	paged-index.c \
	parseopt.c \
	time.c \
	xml.c \
	xmlpp.c \
	\
	arith-bithacks.c \
	\
	blob.c \
	\
	conf.c  \
	conf-parser.l \
	\
	container-array.c \
	container-htbl.c \
	container-list.c \
	container-qhash.c \
	container-qvector.c \
	container-rbtree.c \
	container-ring.c \
	container-slist.c \
	container-qvector.c \
	\
	core-debug.c \
	core-errors.c \
	core-obj.c \
	core-havege.c \
	core-mem.c \
	core-mem-fifo.c \
	core-mem-ring.c \
	core-mem-stack.c \
	core-test.c \
	core-version.c \
	qpage.c \
	qtlsf.c \
	\
	el.c \
	el-stopper.c \
	\
	file.c \
	file-log.c \
	\
	hash-aes.c \
	hash-arc4.c \
	hash-crc32.c \
	hash-crc64.c \
	hash-des.c \
	hash-hash.c \
	hash-md2.c \
	hash-md4.c \
	hash-md5.c \
	hash-padlock.c \
	hash-sha1.c \
	hash-sha2.c \
	hash-sha4.c \
	\
	net-addr.c \
	net-socket.c \
	net-sctp.c \
	\
	property.c \
	property-hash.c \
	\
	qlzo-c.c \
	qlzo-d.c \
	\
	sort-32-64.c \
	\
	str.c \
	str-buf.c \
	str-buf-quoting.c \
	str-buf-gsm.c \
	str-num.c \
	str-conv.c \
	str-conv-ebcdic.c \
	str-ctype.c \
	str-iprintf.c \
	str-dtoa.c \
	str-path.c \
	str-stream.c \
	\
	thr.c \
	thr-evc.c \
	thr-job.c \
	thr-pthread-hook.c \
	\
	tpl.c \
	tpl-funcs.c \
	\
	unix.c \
	unix-fts.c \
	unix-psinfo.c \
	unix-linux.c \
	unix-solaris.c \
	\
	$(end_of_list)

time-lp-simple_SOURCES = time-lp-simple.c

zchk_SOURCES = zchk.c \
	$/lib-common/libcommon.wa \
	$/lib-common/time-lp-simple.a

zchk_LIBS = -lz -lrt -ldl -lpthread

ztst_SOURCES = $(libcommon_SOURCES) time-lp-simple.c ztst.c $/lib-common/compat/check.c
ztst_CFLAGS  = -DCHECK=1
ztst_LIBS = -lz -lrt -ldl

ztst-cfgparser_SOURCES = ztst-cfgparser.c libcommon.a compat/compat.a

ztst-tpl_SOURCES = ztst-tpl.c libcommon.a

ztst-iprintf_SOURCES = ztst-iprintf.c libcommon.a compat/compat.a

ztst-iprintf-fp_CFLAGS = -Wno-format -Wno-missing-format-attribute
ztst-iprintf-fp_SOURCES = ztst-iprintf-fp.c libcommon.a

ztst-iprintf-glibc_CFLAGS = -Wno-format -Wno-missing-format-attribute
ztst-iprintf-glibc_SOURCES = ztst-iprintf-glibc.c libcommon.a

ztst-lzo_SOURCES = ztst-lzo.c libcommon.a compat/compat.a
ztst-lzo_LDFLAGS = -lrt

ztst-hash_SOURCES = ztst-hash.c libcommon.a compat/compat.a

ztst-btree_SOURCES = ztst-btree.c btree.c libcommon.a compat/compat.a
ztst-isndx_SOURCES = ztst-isndx.c libcommon.a compat/compat.a
ztst-sort_SOURCES = ztst-sort.c libcommon.a compat/compat.a
ztst-sort_CFLAGS = -UCHECK
ztst-sort_LIBS = -lm
ztst-iprintf-speed_SOURCES = ztst-iprintf-speed.c libcommon.a compat/compat.a
ztst-iprintf-speed_CFLAGS = -UCHECK

ztst-htbl_SOURCES = ztst-htbl.c libcommon.a compat/compat.a
ztst-path_SOURCES = ztst-path.c libcommon.a compat/compat.a

ifneq (,$(MINGCC))
  # Disable some stuff that does not compile under MingW
  libcommon_SOURCES := $(filter-out unix-linux.c mmappedfile.c btree.c, $(libcommon_SOURCES))
  test_PROGRAMS := $(filter-out btree-dump, $(test_PROGRAMS))
endif

ifneq (SunOS,$(shell uname -s))
DISTCLEANFILES=Upgrading.html
all:: Upgrading.html
Upgrading.html: Upgrading.txt ../Config/asciidoc.conf
	@echo " DOC $(@F)"
	@$(RM) $@
	@asciidoc -f ../Config/asciidoc.conf -b xhtml11 -o $@ $<
	@fromdos $@
endif

include ../Build/base.mk
