##########################################################################
#                                                                        #
#  Copyright (C) 2004-2019 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

none_LIBRARIES = libcommon libcommon-iop
test_PROGRAMS += ztst-cfgparser ztst-tpl ztst-lzo
test_PROGRAMS += ztst-iprintf ztst-iprintf-fp ztst-iprintf-glibc ztst-iprintf-speed
test_PROGRAMS += ztst-qps ztst-qpscheck ztst-qpsstress ztst-hattrie ztst-mem-bench
test_PROGRAMS += ztst-mem
ifeq (,$(TOOLS_REPOSITORY))
none_LIBRARIES += iop-snmp
none_SHARED_LIBRARIES += zchk-iop-plugin core-iop-plugin
test_PROGRAMS += zchk ztst-httpd
endif
iop_PROGRAMS = iop-sign

_IOPJSONPATH = $ljson
_IOPTSPATH = $liop-core

bench_PROGRAMS += zgcd-bench
dso_TOOLS_PROGRAMS = dso-compatibility-check

DISTCLEANFILES = core-version.c
core-version.c: scripts/version.sh FORCE
	$(msg/generate) $@
	$(call fun/gen-and-update-if-changed,$<,$!$@+,$@,rcsid libcommon)

_CFLAGS  = $(libxml2_CFLAGS) $(openssl_CFLAGS) $(valgrind_CFLAGS)
_CFLAGS += $(if $(LIBCOMMON_REPOSITORY),-DLIBCOMMON_REPOSITORY)
_LIBS    = -lz -lrt -ldl -lpthread
_IOPCLASSRANGE = 1-499

ioplibs = \
	core.iop.c \
	ic.iop.c \
	iop-void.c

libcommon_SOURCES = \
	$(ioplibs) \
	\
	licence.blk \
	parseopt.c \
	\
	arith-int.c \
	arith-float.c \
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
	core-rand.c \
	core-mem.blk \
	core-mem-fifo.c \
	core-mem-ring.c \
	core-mem-stack.c \
	core-mem-bench.c \
	core-types.blk \
	core-version.c \
	core-errors.c \
	core-module.c \
	qpage.c \
	\
	el.blk \
	\
	farch.c \
	file.c \
	file-log.blk \
	file-bin.c \
	\
	hash-aes.c \
	hash-arc4.c \
	hash-crc32.c \
	hash-crc64.c \
	hash-des.c \
	hash-hash.c \
	hash-md2.c \
	hash-md5.c \
	hash-padlock.c \
	hash-sha1.c \
	hash-sha2.c \
	hash-sha4.c \
	\
	iop-cfolder.c \
	iop-dso.c \
	iop-json.blk \
	iop-core-obj.c \
	iop-xml-pack.c \
	iop-xml-unpack.c \
	iop-xml-wsdl.blk \
	iop.blk \
	\
	iop-rpc-channel.blk \
	iop-rpc-http-pack.c \
	iop-rpc-http-unpack.c \
	ic.iop.c \
	\
	log.c \
	log-iop.c \
	\
	net-addr.c \
	net-rate.blk \
	net-socket.c \
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
	str-buf-pp.c \
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
	\
	http.c \
	http-srv-static.c \
	http-def.c \
	http-hdr.perf \
	httptokens.c \
	\
	xmlpp.c \
	xmlr.c \
	\
	zlib-wrapper.c \
	ssl.blk \
	\
	z.blk

ifneq ($(HAVE_NETINET_SCTP_H),)
libcommon_SOURCES += net-sctp.c
endif

libcommon_SOURCES += compat/data.c compat/runtime.c
libcommon_NOGENERATED = 1

libcommon-iop_SOURCES = $(ioplibs)

ztst-cfgparser_SOURCES = ztst-cfgparser.c libcommon.a

core-iop-plugin_SOURCES = iop-core/core.iop core-iop-plugin.c

zchk-iop-plugin_SOURCES =  \
    iop-core/ic.iop                                                      \
    zchk-iop-plugin.c                                                    \
    zchk-iop-ressources.c

iop-snmp_SOURCES =  \
	iop-snmp-doc.c \
	iop-snmp-mib.c

zchk_SOURCES = zchk.c \
	zchk-asn1-writer.c \
	zchk-asn1-per.c \
	zchk-bithacks.c \
	zchk-container.blk \
	zchk-hat.blk \
	zchk-iop.c \
	zchk-iop.blk \
	zchk-iop-rpc.c \
	zchk-iop-core-obj.c \
	zchk-licence.c \
	zchk-log.blk \
	zchk-mem.c \
	zchk-str.c \
	zchk-snmp.c \
	zchk-time.c \
	zchk-unix.c \
	zchk-file-log.c \
	zchk-module.c \
	zchk-mem.c \
	zchk-iop-ressources.c \
	zchk-core-obj.c \
	zchk-thrjob.blk \
	zchk-xmlpp.c \
	zchk-farch.fc \
	zchk-farch.c \
	zchk-parseopt.c \
	\
	$ltest-data/snmp/snmp_test.iop \
	$ltest-data/snmp/snmp_test_doc.iop \
	$ltest-data/snmp/snmp_intersec_test.iop \
	\
	$liop-snmp.a \
	$liop/tstiop.a \
	$llibcommon.wa
zchk_LIBS = $(libxml2_LIBS) $(openssl_LIBS) -lm

ztst-httpd_SOURCES = \
	ztst-httpd.c \
	$liop/tstiop.a \
	$llibcommon.a

ztst-httpd_LIBS = $(libxml2_LIBS) $(openssl_LIBS)

ztst-tpl_SOURCES = ztst-tpl.c libcommon.a

ztst-iprintf_SOURCES = ztst-iprintf.c libcommon.a

ztst-iprintf-fp_CFLAGS = -Wno-format -Wno-missing-format-attribute -Wno-format-nonliteral
ztst-iprintf-fp_SOURCES = ztst-iprintf-fp.c libcommon.a

ztst-iprintf-glibc_CFLAGS = -Wno-format -Wno-missing-format-attribute -Wno-format-nonliteral
ztst-iprintf-glibc_SOURCES = ztst-iprintf-glibc.c libcommon.a

ztst-lzo_SOURCES = ztst-lzo.c libcommon.a

ztst-iprintf-speed_SOURCES = ztst-iprintf-speed.c libcommon.a

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

ztst-mem-bench_SOURCES = \
	ztst-mem-bench.c \
	$llibcommon.a

ztst-mem_SOURCES = \
	ztst-mem.blk \
	$llibcommon.a

dso-compatibility-check_SOURCES = \
    dso-compatibility-check.blk \
    $llibcommon.a

iop-sign_SOURCES = \
	iop-sign.blk \
	$llibcommon.a

iop-sign_LIBS = $(openssl_LIBS) $(zlib_LIBS)

include Build/base.mk
