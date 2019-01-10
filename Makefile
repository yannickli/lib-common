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

none_LIBRARIES = libcommon python time-lp-simple
python_SHARED_LIBRARIES += common
test_PROGRAMS += ztst-cfgparser ztst-tpl ztst-lzo ztst-thrjob
test_PROGRAMS += ztst-iprintf ztst-iprintf-fp ztst-iprintf-glibc ztst-iprintf-speed
test_PROGRAMS += ztst-qps ztst-qpscheck ztst-qpsstress ztst-hattrie
ifeq (,$(TOOLS_REPOSITORY))
none_SHARED_LIBRARIES += zchk-tstiop-plugin zchk-iop-plugin core-iop-plugin
test_PROGRAMS += zchk ztst-httpd
endif

bench_PROGRAMS += zgcd-bench

DISTCLEANFILES = core-version.c
core-version.c: scripts/version.sh FORCE
	$(msg/generate) $@
	$< rcsid libcommon > $@+
	$(call fun/update-if-changed,$@+,$@)

_CFLAGS  = $(libxml2_CFLAGS) $(openssl_CFLAGS)
_LIBS    = -lz -lrt -ldl -lpthread

libcommon_SOURCES = \
	farch.c \
	licence.c \
	parseopt.c \
	\
	arith-bithacks.c \
	arith-int.c \
	arith-scan.c \
	\
	asn1.c \
	asn1-writer.c \
	asn1-per.c \
	\
	bit-buf.c \
	bit-wah.c \
	\
	conf.c  \
	conf-parser.l \
	\
	container-qhash.c \
	container-qvector.blk \
	container-rbtree.c \
	container-ring.c \
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
	core-version.c \
	core.iop \
	qpage.c \
	qtlsf.c \
	\
	el.blk \
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
	iop-dso.c \
	iop-json.c \
	iop-xml-pack.c \
	iop-xml-unpack.c \
	iop-xml-wsdl.c \
	iop.blk \
	\
	iop-rpc-channel.c \
	iop-rpc-http-pack.c \
	iop-rpc-http-unpack.c \
	ic.iop \
	\
	net-addr.c \
	net-rate.blk \
	net-socket.c \
	net-sctp.c \
	\
	property.c \
	property-hash.c \
	\
	qlzo-c.c \
	qlzo-d.c \
	\
	qps.blk \
	qps-hat.c \
	qps-bitmap.c \
	\
	sort.blk \
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
	str-l.c \
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
	xmlr.c \
	\
	zlib-wrapper.c \
	ssl.c \
	\
	z.blk

libcommon_SOURCES += compat/compat.c compat/data.c compat/runtime.c

python_SOURCES = python-common.c
python_CFLAGS = $(python2_CFLAGS)
python_LIBS = $(python2_LIBS)

common_SOURCES = python-module.c python.a libcommon.a time-lp-simple.a
common_CFLAGS = $(python2_CFLAGS)
common_LIBS = $(python2_LIBS)

time-lp-simple_SOURCES = time-lp-simple.c

core-iop-plugin_SOURCES = core.iop core-iop-plugin.c
core-iop-plugin_LDFLAGS = -Wl,-z,defs

zchk-iop-plugin_SOURCES = ic.iop zchk-iop-plugin.c 
zchk-iop-plugin_LDFLAGS = -Wl,-z,defs

zchk_SOURCES = zchk.c \
	zchk-asn1-writer.c \
	zchk-bithacks.c \
	zchk-container.blk \
	zchk-hat.blk \
	zchk-iop.c \
	zchk-mem.c \
	zchk-str.c \
	zchk-time.c \
	zchk-unix.c \
	zchk-mem.c \
	$/lib-common/iop/tstiop.a \
	$/lib-common/libcommon.wa \
	$/lib-common/time-lp-simple.a
zchk_LIBS = $(libxml2_LIBS) $(openssl_LIBS) -lm
zchk_LDFLAGS = -rdynamic

ztst-cfgparser_SOURCES = ztst-cfgparser.c libcommon.a

zchk-tstiop-plugin_SOURCES = \
	$/lib-common/iop/tstiop-plugin.c \
	$/lib-common/iop/tstiop.a
zchk-tstiop-plugin_LDFLAGS = -Wl,-z,defs

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

ztst-thrjob_SOURCES = \
	ztst-thrjob.blk \
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a
ztst-thrjob_LIBS = -lm

ztst-qps_SOURCES = \
	ztst-qps.blk \
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a

ztst-qpscheck_SOURCES = \
	ztst-qpscheck.blk \
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a

ztst-qpsstress_SOURCES = \
	ztst-qpsstress.blk \
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a
ztst-qpsstress_LIBS = -lm

ztst-hattrie_SOURCES = \
	ztst-hattrie.blk \
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a

zgcd-bench_SOURCES = \
	zgcd-bench.c \
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a

ifneq (SunOS,$(shell uname -s))
DISTCLEANFILES=Upgrading.html
all:: Upgrading.html
Upgrading.html: Upgrading.txt Config/asciidoc.conf
	@echo " DOC $(@F)"
	@$(RM) $@
	@asciidoc -f Config/asciidoc.conf -b xhtml11 -o $@ $<
	@dos2unix $@
endif

include Build/base.mk
