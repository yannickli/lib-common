##########################################################################
#                                                                        #
#  Copyright (C) 2004-2018 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

import os
import sys

waftoolsdir = os.path.join(os.getcwd(), 'waftools')
sys.path.insert(0, waftoolsdir)

import waftools.intersec as intersec


# FIXME:
#   - waf build -v reports warnings about nodes created more than once
#   - __FILE__ are wrong (this is why zchk executes no test)
#   - clean cflags, support profiles (default, debug, release, asan, ...)
#   - enhance configure (be equivalent to Make's one)
#   - Fix various TODOs and FIXMEs in the wscript files

# {{{ options

def options(ctx):
    ctx.load('compiler_c')

# }}}
# {{{ configure

def configure(ctx):
    # Loads only waf definitions
    ctx.load('intersec', tooldir=waftoolsdir)

    # Setup gcc as default compiler
    ctx.load('compiler_c')

    # Scripts
    ctx.find_program('_run_checks.sh',
                     path_list=[os.path.join(ctx.path.abspath(), 'Build')],
                     mandatory=True,
                     var='RUN_CHECKS_SH')
    ctx.recurse('scripts')

    # External programs
    ctx.find_program('gperf', mandatory=True)

    # External libraries
    ctx.check_cfg(package='libxml-2.0', uselib_store='libxml',
                  args=['--cflags', '--libs'])

    ctx.check_cfg(package='openssl', uselib_store='openssl',
                  args=['--cflags', '--libs'])

    ctx.check_cfg(package='zlib', uselib_store='zlib',
                  args=['--cflags', '--libs'])


    # TODO: Must be cleanup depending on the chosen C compiler (test each one
    # of them).
    ctx.env.CFLAGS = [
        '-std=gnu11',
        '-fdiagnostics-show-option',
        '-funsigned-char',
        '-fno-strict-aliasing',
        '-fwrapv',
        '-Wall',
        '-Wextra',
        '-Werror',
        '-Wno-error=deprecated-declarations',
        '-Wchar-subscripts',
        '-Wundef',
        '-Wshadow',
        '-Wwrite-strings',
        '-Wsign-compare',
        '-Wunused',
        '-Wno-unused-parameter',
        '-Wuninitialized',
        '-Winit-self',
        '-Wpointer-arith',
        '-Wredundant-decls',
        '-Wno-format-y2k',
        '-Wmissing-format-attribute',
        '-Wstrict-prototypes',
        '-Wmissing-prototypes',
        '-Wmissing-declarations',
        '-Wnested-externs',
        '-Wdeclaration-after-statement',
        '-Wno-format-zero-length',
        '-D_GNU_SOURCE',
        '-I/usr/include/valgrind',
        '-DHAS_LIBCOMMON_REPOSITORY=0',
        '-DWAF_MODE',
        '-fPIC',
    ]

    # Find clang for blk
    ctx.env.CLANG = ctx.find_program('clang')
    ctx.env.CLANG_REWRITE_FLAGS = [
        '-cc1',
        '-x', 'c',
        '-std=gnu11',
        '-D_GNU_SOURCE',
        '-fblocks',
        '-I/usr/include/valgrind',
        '-DHAS_LIBCOMMON_REPOSITORY=0',
        '-DWAF_MODE',
        '-fdiagnostics-show-option',
        '-fwrapv',
        '-Wall',
        '-Wextra',
        '-Werror',
        '-Wno-error=deprecated-declarations',
        '-Wno-gnu-designator',
        '-Wno-return-type-c-linkage',
        '-Wbool-conversion',
        '-Wempty-body',
        '-Wloop-analysis',
        '-Wsizeof-array-argument',
        '-Wstring-conversion',
        '-Wparentheses',
        '-Wduplicate-enum',
        '-Wheader-guard',
        '-Wlogical-not-parentheses',
        '-Wno-nullability-completeness',
        '-Wno-shift-negative-value',
        '-Wcomma',
        '-Wfloat-overflow-conversion',
        '-Wfloat-zero-conversion',
        '-Wchar-subscripts',
        '-Wundef',
        '-Wshadow',
        '-Wwrite-strings',
        '-Wsign-compare',
        '-Wunused',
        '-Wno-unused-parameter',
        '-Wuninitialized',
        '-Winit-self',
        '-Wpointer-arith',
        '-Wredundant-decls',
        '-Wno-format-y2k',
        '-Wmissing-format-attribute',
        '-Wstrict-prototypes',
        '-Wmissing-prototypes',
        '-Wmissing-declarations',
        '-Wnested-externs',
        '-Wdeclaration-after-statement',
        '-Wno-format-zero-length',
        '-internal-isystem', '/usr/local/include',
        '-internal-isystem', '/srv/tools/clang_3.9/bin/../lib/clang/3.9.1/include',
        '-internal-externc-isystem', '/usr/include/x86_64-linux-gnu',
        '-internal-externc-isystem', '/include',
        '-internal-externc-isystem', '/usr/include',
        '-rewrite-blocks',
    ]

# }}}
# {{{ build

def build(ctx):
    # Register Intersec post functions
    ctx.add_post_fun(intersec.deploy_targets)
    ctx.add_post_fun(intersec.run_checks)

    # Declare 3 build groups:
    #  - one for generating the build tools (iopc)
    #  - one for generating the code (IOP files processing)
    #  - one for compiling code after then.
    #
    # This way we are sure IOPs are generated before the code using them
    # is compiled. Refer to section "Building the compiler first" of the
    # waf book.
    ctx.add_group('tools_generation')
    ctx.add_group('code_generation')
    ctx.add_group('code_compiling')

    ctx.set_group('tools_generation')

    # {{{ libcommon library

    ctx(rule='${VERSION_SH} rcsid libcommon > ${TGT}',
        target='core-version.c', always=True)

    # TODO: add net-sctp.c in libcommon target
    ctx.stlib(target='libcommon',
        includes='.',
        export_includes=['.'],
        depends_on='core-version.c',
        use=['libxml', 'openssl', 'zlib', 'compat'],
        lib=['pthread', 'dl'],
        source=[
            'core-version.c',
            'core.iop.c',
            'ic.iop.c',
            'iop-void.c',
            'log-iop.c',

            'arith-int.c',
            'arith-float.c',
            'arith-scan.c',
            'asn1.c',
            'asn1-writer.c',
            'asn1-per.c',

            'bit-buf.c',
            'bit-wah.c',

            'conf.c',
            'conf-parser.l',
            'container-qhash.c',
            'container-qvector.blk',
            'container-rbtree.c',
            'container-ring.c',
            'core-bithacks.c',
            'core-obj.c',
            'core-rand.c',
            'core-mem-fifo.c',
            'core-mem-ring.c',
            'core-mem-stack.c',
            'core-mem-bench.c',
            'core-errors.c',
            'core-mem.blk',
            'core-module.c',
            'core-types.blk',

            'compat/compat.c',
            'compat/data.c',
            'compat/runtime.c',

            'datetime.c',
            'datetime-iso8601.c',

            'el.blk',

            'farch.c',
            'file.c',
            'file-bin.c',
            'file-log.blk',

            'hash-aes.c',
            'hash-arc4.c',
            'hash-crc32.c',
            'hash-crc64.c',
            'hash-des.c',
            'hash-hash.c',
            'hash-md2.c',
            'hash-md5.c',
            'hash-padlock.c',
            'hash-sha1.c',
            'hash-sha2.c',
            'hash-sha4.c',
            'http.c',
            'http-hdr.perf',
            'http-srv-static.c',
            'http-def.c',
            'httptokens.c',

            'iop.blk',
            'iop-cfolder.c',
            'iop-dso.c',
            'iop-json.blk',
            'iop-xml-pack.c',
            'iop-xml-unpack.c',
            'iop-xml-wsdl.blk',
            'iop-rpc-channel.blk',
            'iop-rpc-http-pack.c',
            'iop-rpc-http-unpack.c',

            'licence.blk',
            'log.c',

            'net-addr.c',
            'net-socket.c',
            'net-rate.blk',

            'parseopt.c',
            'property.c',
            'property-hash.c',

            'qlzo-c.c',
            'qlzo-d.c',
            'qpage.c',
            'qps.blk',
            'qps-hat.c',
            'qps-bitmap.c',

            'sort.blk',
            'ssl.blk',
            'str.c',
            'str-buf-gsm.c',
            'str-buf-quoting.c',
            'str-buf-pp.c',
            'str-buf.c',
            'str-conv-ebcdic.c',
            'str-conv.c',
            'str-ctype.c',
            'str-dtoa.c',
            'str-iprintf.c',
            'str-l.c',
            'str-num.c',
            'str-outbuf.c',
            'str-path.c',
            'str-stream.c',

            'thr.c',
            'thr-evc.c',
            'thr-job.blk',
            'thr-spsc.c',
            'tpl.c',
            'tpl-funcs.c',

            'unix.blk',
            'unix-fts.c',
            'unix-psinfo.c',
            'unix-linux.c',

            'xmlpp.c',
            'xmlr.c',

            'z.blk',
            'zlib-wrapper.c',
        ]
    )

    # }}}

    ctx.recurse('compat')
    ctx.recurse('scripts')
    ctx.recurse('tools')
    ctx.recurse('iopc')

    ctx.set_group('code_compiling')

    ctx.recurse('iop')
    ctx.recurse('iop-tutorial')

    iop_class_range = '1-499'

    # {{{ iop-snmp library

    ctx.stlib(target='iop-snmp', source=[
        'iop-snmp-doc.c',
        'iop-snmp-mib.c',
    ], use='libcommon')

    # }}}
    # {{{ dso-compatibility-check / iop-sign

    ctx.program(target='dso-compatibility-check', features='c cprogram',
                source='dso-compatibility-check.blk',
                use='libcommon',
                lib=['pthread', 'dl'])

    ctx.program(target='iop-sign', features='c cprogram',
                source='iop-sign.blk',
                use='libcommon',
                lib=['pthread', 'dl'])

    # }}}
    # {{{ zchk and ztst-*

    # FIXME: iop.iop_dso_fixup and iop.iop_dso_fixup_bad_dep are skipped
    #        because zchk-tstiop2-plugin.so fails to be dlopened:
    #        undefined symbol: tstiop__my_struct_a__s
    ctx.program(target='zchk',
        source=[
            'zchk.c',
            'zchk-asn1-writer.c',
            'zchk-asn1-per.c',
            'zchk-bithacks.c',
            'zchk-iop.c',
            'zchk-iop-rpc.c',
            'zchk-licence.c',
            'zchk-str.c',
            'zchk-snmp.c',
            'zchk-time.c',
            'zchk-unix.c',
            'zchk-file-log.c',
            'zchk-module.c',
            'zchk-mem.c',
            'zchk-iop-ressources.c',
            'zchk-core-obj.c',
            'zchk-xmlpp.c',

            'zchk-container.blk',
            'zchk-hat.blk',
            'zchk-iop.blk',
            'zchk-log.blk',
            'zchk-thrjob.blk',

            'test-data/snmp/snmp_test.iop',
            'test-data/snmp/snmp_test_doc.iop',
            'test-data/snmp/snmp_intersec_test.iop'
        ],
        use='iop-snmp tstiop',
        use_whole='libcommon',
        iop_class_range=iop_class_range,
        lib=['pthread', 'dl', 'm'], includes='.')

    ctx.shlib(target='zchk-iop-plugin', source=[
        'iop-core/ic.iop',
        'zchk-iop-plugin.c',
        'zchk-iop-ressources.c',
    ], use='libcommon', iop_class_range=iop_class_range)

    ctx.program(target='ztst-cfgparser', source='ztst-cfgparser.c',
                use='libcommon tstiop', lib=['pthread', 'dl'])

    ctx.program(target='ztst-httpd', source='ztst-httpd.c',
                use='libcommon tstiop', lib=['pthread', 'dl'])

    ctx.program(target='ztst-tpl', source='ztst-tpl.c',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='ztst-iprintf', source='ztst-iprintf.c',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='ztst-iprintf-fp', source='ztst-iprintf-fp.c',
                use='libcommon', lib=['pthread', 'dl'],
                cflags=['-Wno-format', '-Wno-missing-format-attribute',
                        '-Wno-format-nonliteral'])

    ctx.program(target='ztst-iprintf-glibc', source='ztst-iprintf-glibc.c',
                use='libcommon', lib=['pthread', 'dl'],
                cflags=['-Wno-format', '-Wno-missing-format-attribute',
                        '-Wno-format-nonliteral'])

    ctx.program(target='ztst-iprintf-speed', source='ztst-iprintf-speed.c',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='ztst-lzo', source='ztst-lzo.c',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='ztst-qps', features="c",
                source='ztst-qps.blk',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='ztst-qpscheck', features="c",
                source='ztst-qpscheck.blk',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='ztst-qpsstress', features="c",
                source='ztst-qpsstress.blk',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='ztst-hattrie', features="c",
                source='ztst-hattrie.blk',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='zgcd-bench', source='zgcd-bench.c',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='ztst-mem-bench', source='ztst-mem-bench.c',
                use='libcommon', lib=['pthread', 'dl'])

    ctx.program(target='ztst-mem', features="c",
                source='ztst-mem.blk',
                use='libcommon', lib=['pthread', 'dl'])

    # }}}
# }}}
