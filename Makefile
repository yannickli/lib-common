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
test_PROGRAMS += ztst-cfgparser ztst-tpl ztst-lzo
test_PROGRAMS += ztst-iprintf ztst-iprintf-fp ztst-iprintf-glibc ztst-iprintf-speed
test_PROGRAMS += ztst-qps ztst-qpscheck ztst-qpsstress ztst-hattrie
ifeq (,$(TOOLS_REPOSITORY))
none_SHARED_LIBRARIES += zchk-tstiop-plugin zchk-iop-plugin core-iop-plugin
test_PROGRAMS += zchk ztst-httpd
endif

_IOPJSONPATH = $ljson

bench_PROGRAMS += zgcd-bench

DISTCLEANFILES = core-version.c
core-version.c: scripts/version.sh FORCE
	$(msg/generate) $@
	$(call fun/gen-and-update-if-changed,$<,$!$@+,$@,rcsid libcommon)

_CFLAGS  = $(libxml2_CFLAGS) $(openssl_CFLAGS)
_CFLAGS += $(if $(LIBCOMMON_REPOSITORY),-DLIBCOMMON_REPOSITORY)
_LIBS    = -lz -lrt -ldl -lpthread
_IOPCLASSRANGE = 1-499

libcommon_SOURCES = \
	licence.blk \
	parseopt.c \
	\
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
	core-bithacks.c \
	core-obj.c \
	core-havege.c \
	core-mem.c \
	core-mem-fifo.c \
	core-mem-ring.c \
	core-mem-stack.c \
	core-version.c \
	core.iop \
	core-module.c \
	qpage.c \
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
	iop-xml-wsdl.blk \
	iop.blk \
	\
	iop-rpc-channel.c \
	iop-rpc-http-pack.c \
	iop-rpc-http-unpack.c \
	ic.iop \
	\
	log.c \
	log-iop.c \
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
	thr-evc.c \
	thr-job.blk \
	thr-spsc.c \
	\
	datetime.c \
	datetime-iso8601.c \
	\
	tpl.c \
	tpl-funcs.c \
	\
	unix.blk \
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

# time-lp-simple.a still needs to be generated because submodules are
# linking to it, when these submodules will be updated this line should
# be removed
time-lp-simple_SOURCES = time-lp-simple.c

python_SOURCES = python-common.c
python_CFLAGS = $(python2_CFLAGS)
python_LIBS = $(python2_LIBS)

common_SOURCES = python-module.c python.a libcommon.a
common_CFLAGS = $(python2_CFLAGS)
common_LIBS = $(python2_LIBS)

ztst-cfgparser_SOURCES = ztst-cfgparser.c libcommon.a

ifeq (,$(TOOLS_REPOSITORY))
core-iop-plugin_SOURCES = core.iop core-iop-plugin.c
core-iop-plugin_LDFLAGS = -Wl,-z,defs

zchk-iop-plugin_SOURCES = ic.iop zchk-iop-plugin.c zchk-iop-ressources.c
zchk-iop-plugin_LDFLAGS = -Wl,-z,defs

zchk_SOURCES = zchk.c \
	zchk-asn1-writer.c \
	zchk-asn1-per.c \
	zchk-bithacks.c \
	zchk-container.blk \
	zchk-hat.blk \
	zchk-iop.c \
	zchk-iop-rpc.c \
	zchk-licence.c \
	zchk-mem.c \
	zchk-str.c \
	zchk-time.c \
	zchk-unix.c \
	zchk-file-log.c \
	zchk-module.c \
	zchk-mem.c \
	zchk-iop-ressources.c \
	zchk-parseopt.c \
	$liop/tstiop.a \
	$llibcommon.wa
zchk_LIBS = $(libxml2_LIBS) $(openssl_LIBS) -lm
zchk_LDFLAGS = -rdynamic

zchk-tstiop-plugin_SOURCES = \
	$liop/tstiop-plugin.c \
	$liop/tstiop.a
zchk-tstiop-plugin_LDFLAGS = -Wl,-z,defs

ztst-httpd_SOURCES = \
	ztst-httpd.c \
	$liop/tstiop.a \
	$llibcommon.a

ztst-httpd_LIBS = $(libxml2_LIBS)
endif

ztst-tpl_SOURCES = ztst-tpl.c libcommon.a time-lp-simple.a

ztst-iprintf_SOURCES = ztst-iprintf.c libcommon.a time-lp-simple.a

ztst-iprintf-fp_CFLAGS = -Wno-format -Wno-missing-format-attribute -Wno-format-nonliteral
ztst-iprintf-fp_SOURCES = ztst-iprintf-fp.c libcommon.a time-lp-simple.a

ztst-iprintf-glibc_CFLAGS = -Wno-format -Wno-missing-format-attribute -Wno-format-nonliteral
ztst-iprintf-glibc_SOURCES = ztst-iprintf-glibc.c libcommon.a time-lp-simple.a

ztst-lzo_SOURCES = ztst-lzo.c libcommon.a time-lp-simple.a

ztst-iprintf-speed_SOURCES = ztst-iprintf-speed.c libcommon.a time-lp-simple.a

ztst-qps_SOURCES = \
	ztst-qps.blk \
	$llibcommon.a

ztst-qpscheck_SOURCES = \
	ztst-qpscheck.blk \
	$llibcommon.a

ztst-qpsstress_SOURCES = \
	ztst-qpsstress.blk \
	$llibcommon.a

ztst-qpsstress_LIBS = -lm

ztst-hattrie_SOURCES = \
	ztst-hattrie.blk \
	$llibcommon.a

zgcd-bench_SOURCES = \
	zgcd-bench.c \
	$llibcommon.a

include Build/base.mk
