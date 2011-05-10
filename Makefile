##########################################################################
#                                                                        #
#  Copyright (C) 2004-2011 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

none_LIBRARIES = libcommon time-lp-simple
test_PROGRAMS += zchk ztst-cfgparser ztst-tpl ztst-lzo ztst-thrjob
test_PROGRAMS += ztst-iprintf ztst-iprintf-fp ztst-iprintf-glibc ztst-iprintf-speed
ifeq (,$(TOOLS_REPOSITORY))
test_PROGRAMS += ztst-iop ztst-httpd
endif

DISTCLEANFILES = core-version.c
core-version.c: scripts/version.sh FORCE
	$(msg/generate) $@
	$< rcsid libcommon > $@+
	$(call fun/update-if-changed,$@+,$@)

_CFLAGS  = $(libxml2_CFLAGS)
_LIBS    = -lz -lrt -ldl -lpthread

libcommon_SOURCES = \
	farch.c \
	licence.c \
	parseopt.c \
	\
	arith-bithacks.c \
	\
	asn1.c \
	asn1-writer.c \
	asn1-per.c \
	ztst-asn1-writer.c \
	\
	bit-stream.c \
	bit-buf.c \
	\
	conf.c  \
	conf-parser.l \
	\
	container-list.c \
	container-qhash.c \
	container-qvector.blk \
	container-rbtree.c \
	container-ring.c \
	container-slist.c \
	\
	core-debug.c \
	core-errors.c \
	core-obj.c \
	core-havege.c \
	core-mem.c \
	core-mem-debug.c \
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
	iop-cfolder.c \
	iop-json.c \
	iop-xml-pack.c \
	iop-xml-unpack.c \
	iop-xml-wsdl.c \
	iop.c \
	\
	iop-rpc-channel.c \
	iop-rpc-http-pack.c \
	iop-rpc-http-unpack.c \
	ic.iop.c \
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
	str-buf-gsm.c \
	str-buf-quoting.c \
	str-buf.c \
	str-conv-ebcdic.c \
	str-conv.c \
	str-ctype.c \
	str-dtoa.c \
	str-iprintf.c \
	str-num.c \
	str-outbuf.c \
	str-path.c \
	str-stream.c \
	\
	thr.c \
	thr-pthread-hook.c \
	thr-evc.c \
	thr-job.blk \
	thr-spsc.c \
	\
	time.c \
	time-iso8601.c \
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
	http.c \
	http-srv-static.c \
	http-def.c \
	http-hdr.perf \
	http.tokens \
	\
	xmlpp.c \
	xmlr.c

libcommon_SOURCES += compat/compat.c compat/data.c compat/runtime.c

time-lp-simple_SOURCES = time-lp-simple.c

zchk_SOURCES = zchk.c \
	$/lib-common/libcommon.wa \
	$/lib-common/time-lp-simple.a
zchk_LIBS = $(libxml2_LIBS)

ztst_SOURCES = $(libcommon_SOURCES) time-lp-simple.c ztst.c $/lib-common/compat/check.c
ztst_CFLAGS  = -DCHECK=1

ztst-cfgparser_SOURCES = ztst-cfgparser.c libcommon.a

ztst-iop_SOURCES = \
	ztst-iop.c \
	$/lib-common/iop/tstiop.a \
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a
ztst-iop_LIBS = $(libxml2_LIBS)

ztst-httpd_SOURCES = \
	ztst-httpd.c \
	$/lib-common/iop/tstiop.a \
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a
ztst-httpd_LIBS = $(libxml2_LIBS)

ztst-tpl_SOURCES = ztst-tpl.c libcommon.a

ztst-iprintf_SOURCES = ztst-iprintf.c libcommon.a

ztst-iprintf-fp_CFLAGS = -Wno-format -Wno-missing-format-attribute -Wno-format-nonliteral
ztst-iprintf-fp_SOURCES = ztst-iprintf-fp.c libcommon.a

ztst-iprintf-glibc_CFLAGS = -Wno-format -Wno-missing-format-attribute -Wno-format-nonliteral
ztst-iprintf-glibc_SOURCES = ztst-iprintf-glibc.c libcommon.a

ztst-lzo_SOURCES = ztst-lzo.c libcommon.a

ztst-iprintf-speed_SOURCES = ztst-iprintf-speed.c libcommon.a
ztst-iprintf-speed_CFLAGS = -UCHECK

ztst-thrjob_SOURCES = \
	ztst-thrjob.blk \
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a
ztst-thrjob_LIBS = -lrt -ldl -lpthread -lm

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
