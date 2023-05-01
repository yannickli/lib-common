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
# pylint: disable = invalid-name, bad-continuation

import os
import sys

# pylint: disable = import-error
from waflib import Context, Logs, Errors
# pylint: enable = import-error

waftoolsdir = os.path.join(os.getcwd(), 'waftools')
sys.path.insert(0, waftoolsdir)


out = ".build-waf-%s" % os.environ.get('P', 'default')


# {{{ options


def load_tools(ctx):
    ctx.load('common',  tooldir=waftoolsdir)
    ctx.load('backend', tooldir=waftoolsdir)
    if sys.version_info >= (2, 7):
        # This extension uses the python json library to unpack the existing
        # compilation database. But it uses an argument that is not compatible
        # with older python versions.
        ctx.load('compilation_database', tooldir=waftoolsdir)
    for tool in getattr(ctx, 'extra_waftools', []):
        ctx.load(tool, tooldir=waftoolsdir)

    # Configure waf to re-evaluate hashes only when file timestamp/size
    # change. This is way faster on no-op builds.
    ctx.load('md5_tstamp')


def options(ctx):
    load_tools(ctx)


# }}}
# {{{ configure


def configure(ctx):
    load_tools(ctx)

    # Export includes
    ctx.register_global_includes(['.', 'compat'])

    # {{{ Compilation flags

    flags = ['-DHAS_LIBCOMMON_REPOSITORY=0']

    ctx.env.CFLAGS += flags
    ctx.env.CXXFLAGS += flags
    ctx.env.CLANG_FLAGS += flags
    ctx.env.CLANG_REWRITE_FLAGS += flags
    ctx.env.CLANGXX_FLAGS += flags
    ctx.env.CLANGXX_REWRITE_FLAGS += flags

    # }}}
    # {{{ Dependencies

    # Scripts
    ctx.recurse('scripts')

    # External programs
    ctx.find_program('gperf')

    # External libraries
    ctx.check_cfg(package='libxml-2.0', uselib_store='libxml',
                  args=['--cflags', '--libs'])
    ctx.check_cfg(package='openssl', uselib_store='openssl',
                  args=['--cflags', '--libs'])
    ctx.check_cfg(package='zlib', uselib_store='zlib',
                  args=['--cflags', '--libs'])
    ctx.check_cfg(package='valgrind', uselib_store='valgrind',
                  args=['--cflags'])

    # Linux UAPI SCTP header
    sctp_h = '/usr/include/linux/sctp.h'
    if os.path.exists(sctp_h):
        sctp_flag = '-DHAVE_LINUX_UAPI_SCTP_H'
        ctx.env.CFLAGS.append(sctp_flag)
        ctx.env.CLANG_FLAGS.append(sctp_flag)
        ctx.env.CLANG_REWRITE_FLAGS.append(sctp_flag)
        ctx.msg('Checking for Linux UAPI SCTP header', sctp_h)
    else:
        Logs.info('missing Linux UAPI SCTP header,'
                  ' it will be replaced by a custom one')

    # {{{ Python 2

    # TODO waf: use waf python tool for that?
    ctx.find_program('python2')

    # Check version is >= 2.6
    py_ver = ctx.cmd_and_log(ctx.env.PYTHON2 + ['--version'],
                             output=Context.STDERR)
    py_ver = py_ver.strip()[len('Python '):]
    py_ver_minor = int(py_ver.split('.')[1])
    if py_ver_minor not in [6, 7]:
        ctx.fatal('unsupported python version {0}'.format(py_ver))

    # Get compilation flags
    if py_ver_minor == 6:
        ctx.find_program('python2.6-config', var='PYTHON2_CONFIG')
    else:
        ctx.find_program('python2.7-config', var='PYTHON2_CONFIG')

    py_cflags = ctx.cmd_and_log(ctx.env.PYTHON2_CONFIG + ['--includes'])
    ctx.env.append_unique('CFLAGS_python2', py_cflags.strip().split(' '))

    py_prefix = ctx.cmd_and_log(ctx.env.PYTHON2_CONFIG + ['--prefix'])
    ctx.env.append_unique('LDFLAGS_python2',
                          '-L{0}/lib'.format(py_prefix.strip()))
    py_ldflags = ctx.cmd_and_log(ctx.env.PYTHON2_CONFIG + ['--ldflags'])
    ctx.env.append_unique('LDFLAGS_python2', py_ldflags.strip().split(' '))

    # }}}
    # {{{ Python 3

    try:
        ctx.find_program(['python3-config', 'python3.6-config'],
                         var='PYTHON3_CONFIG')
    except Errors.ConfigurationError as e:
        Logs.debug('cannot configure python3: %s', e.msg)
    else:
        py_cflags = ctx.cmd_and_log(ctx.env.PYTHON3_CONFIG + ['--includes'])
        ctx.env.append_unique('CFLAGS_python3', py_cflags.strip().split(' '))

        py_ldflags = ctx.cmd_and_log(ctx.env.PYTHON3_CONFIG + ['--ldflags'])
        ctx.env.append_unique('LDFLAGS_python3',
                              py_ldflags.strip().split(' '))

    # }}}

    # }}}


# }}}
# {{{ build


def build(ctx):
    # Declare 4 build groups:
    #  - one for generating the "version" source files
    #  - one for compiling farchc
    #  - one for compiling iopc
    #  - one for compiling pxc (used in the tools repository)
    #  - one for generating/compiling code after then.
    #
    # This way we are sure farchc is generated before iopc (needed because it
    # uses a farch file), and iopc is generated before building the IOP files.
    # Refer to section "Building the compiler first" of the waf book.
    ctx.add_group('gen_version')
    ctx.add_group('farchc')
    ctx.add_group('iopc')
    ctx.add_group('pxcc')
    ctx.add_group('code_compiling')

    load_tools(ctx)

    ctx.set_group('farchc')

    # {{{ libcommon library

    with ctx.UseGroup(ctx, 'gen_version'):
        ctx(rule='${VERSION_SH} rcsid libcommon > ${TGT}',
            target='core-version.c', cwd='.', always=True)

    ctx.stlib(target='libcommon',
        depends_on='core-version.c',
        use=['libxml', 'openssl', 'zlib', 'valgrind', 'compat'],
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
            'iop-core-obj.c',
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
            'net-sctp.c',
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

    ctx.recurse('tools')
    ctx.recurse('iopc')

    ctx.set_group('code_compiling')

    ctx.recurse('iop')
    ctx.recurse('iop-tutorial')
    ctx.recurse('iop-core')
    ctx.recurse('test-data/snmp')

    ctx.IopcOptions(ctx, class_range='1-499',
                    json_path='json',
                    ts_path='iop-core')

    # {{{ iop-snmp library

    ctx.stlib(target='iop-snmp', source=[
        'iop-snmp-doc.c',
        'iop-snmp-mib.c',
    ], use='libcommon')

    # }}}
    # {{{ dso-compatibility-check / iop-sign

    ctx.program(target='dso-compatibility-check', features='c cprogram',
                source='dso-compatibility-check.blk',
                use='libcommon')

    ctx.program(target='iop-sign', features='c cprogram',
                source='iop-sign.blk',
                use='libcommon')

    # }}}
    # {{{ zchk and ztst-*

    ctx.stlib(target='zchk-iop-ressources', source='zchk-iop-ressources.c')

    ctx.program(target='zchk',
        source=[
            'zchk.c',
            'zchk-asn1-writer.c',
            'zchk-asn1-per.c',
            'zchk-bithacks.c',
            'zchk-iop.c',
            'zchk-iop-rpc.c',
            'zchk-iop-core-obj.c',
            'zchk-licence.c',
            'zchk-parseopt.c',
            'zchk-str.c',
            'zchk-snmp.c',
            'zchk-time.c',
            'zchk-unix.c',
            'zchk-farch.c',
            'zchk-file-log.c',
            'zchk-module.c',
            'zchk-mem.c',
            'zchk-core-obj.c',
            'zchk-xmlpp.c',

            'zchk-farch.fc',

            'zchk-container.blk',
            'zchk-hat.blk',
            'zchk-qps-bitmap.c',
            'zchk-iop.blk',
            'zchk-log.blk',
            'zchk-thrjob.blk',
        ],
        use=[
            'iop-snmp',
            'tstiop',
            'tst-snmp-iop',
            'zchk-iop-ressources',
        ], use_whole='libcommon')

    ctx.shlib(target='zchk-iop-plugin', source=[
        'zchk-iop-plugin.c',
    ], use=[
        'libcommon',
        'zchk-iop-ressources',
    ], remove_dynlibs=True)

    ctx.program(target='ztst-cfgparser', source='ztst-cfgparser.c',
                use='libcommon tstiop')

    ctx.program(target='ztst-httpd', source='ztst-httpd.c',
                use='libcommon tstiop')

    ctx.program(target='ztst-tpl', source='ztst-tpl.c',
                use='libcommon')

    ctx.program(target='ztst-iprintf', source='ztst-iprintf.c',
                use='libcommon')

    ctx.program(target='ztst-iprintf-fp', source='ztst-iprintf-fp.c',
                use='libcommon',
                cflags=['-Wno-format', '-Wno-missing-format-attribute',
                        '-Wno-format-nonliteral'])

    ctx.program(target='ztst-iprintf-glibc', source='ztst-iprintf-glibc.c',
                use='libcommon',
                cflags=['-Wno-format', '-Wno-missing-format-attribute',
                        '-Wno-format-nonliteral'])

    ctx.program(target='ztst-iprintf-speed', source='ztst-iprintf-speed.c',
                use='libcommon')

    ctx.program(target='ztst-lzo', source='ztst-lzo.c', use='libcommon')

    ctx.program(target='ztst-qps', features="c cprogram",
                source='ztst-qps.blk', use='libcommon')

    ctx.program(target='ztst-qps-bitmap-bench', features="c cprogram",
                source='ztst-qps-bitmap-bench.c', use="libcommon")

    ctx.program(target='ztst-qpscheck', features="c cprogram",
                source='ztst-qpscheck.blk', use='libcommon')

    ctx.program(target='ztst-qpsstress', features="c cprogram",
                source='ztst-qpsstress.blk', use='libcommon')

    ctx.program(target='ztst-hattrie', features="c cprogram",
                source='ztst-hattrie.blk', use='libcommon')

    ctx.program(target='zgcd-bench', source='zgcd-bench.c', use='libcommon')

    ctx.program(target='ztst-mem-bench', source='ztst-mem-bench.c',
                use='libcommon')

    ctx.program(target='ztst-mem', features="c cprogram",
                source='ztst-mem.blk', use='libcommon')

    ctx.program(target='container-bench', features="c cprogram",
                source='container-bench.c', use='libcommon')

    # }}}


# }}}
