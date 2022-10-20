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

'''
Contains the code needed for backend compilation.
'''

import os
import re
import copy
from itertools import chain

# pylint: disable = import-error
from waflib import TaskGen, Utils, Context, Errors, Options, Logs

from waflib.Build import BuildContext
from waflib.Configure import ConfigurationContext
from waflib.Task import Task
from waflib.TaskGen import extension
from waflib.Tools import c as c_tool
from waflib.Tools import c_preproc
from waflib.Tools import cxx
from waflib.Tools import ccroot
# pylint: enable = import-error


# {{{ use_whole

# These functions implement the use_whole attribute, allowing to link a
# library with -whole-archive

@TaskGen.feature('c', 'cprogram', 'cstlib')
@TaskGen.before_method('process_rule')
def prepare_whole(self):
    use_whole = self.to_list(getattr(self, 'use_whole', []))
    if not use_whole:
        return

    # Add the 'use_whole' elements in the 'use' list, so that waf considers it
    # for paths, includes, ...
    self.use = list(self.to_list(getattr(self, 'use', [])))
    for uw in use_whole:
        if  not getattr(self, 'no_use_whole_error', False) \
        and uw in self.use:
            self.bld.fatal(('`{0}` from `use_whole` of target `{1}` is '
                            'already in attribute `use`, you may remove it')
                           .format(uw, self.target))
        self.use.append(uw)

@TaskGen.feature('c', 'cprogram', 'cstlib')
@TaskGen.after_method('process_use')
def process_whole(self):
    use_whole = self.to_list(getattr(self, 'use_whole', []))
    if not use_whole:
        return

    # Patch the LINKFLAGS to enter in whole archive mode...
    self.env.append_value('LINKFLAGS',
                          '-Wl,--as-needed,--whole-archive')

    cwd = self.get_cwd()

    for name in use_whole:
        # ...add the whole archive libraries...
        lib_task = self.bld.get_tgen_by_name(name)
        self.env.STLIB.remove(name)

        # TODO waf: filter STLIB_PATH by removing unused ones.
        self.env.append_value(
            'LINKFLAGS',
            list(chain.from_iterable(('-Xlinker', p.path_from(cwd))
                                     for p in lib_task.link_task.outputs)))

    # ...and close the whole archive mode
    self.env.append_value('LINKFLAGS', '-Wl,--no-whole-archive')

# }}}
# {{{ Filter-out zchk binaries in release mode

"""
Tests are not compiled in release mode, so compiling zchk programs is useless
then. This code filters them out.

It assumes all C task generators whose name begins with `zchk` are dedicated
to tests (and only them).
"""

def filter_out_zchk(ctx):
    for g in ctx.groups:
        for i in range(len(g) - 1, 0, -1):
            tgen = g[i]
            features = tgen.to_list(getattr(tgen, 'features', []))
            if  tgen.name.startswith('zchk') and 'c' in features:
                del g[i]


# }}}
# {{{ fPIC compilation for shared libraries

"""
Our shared libraries need to be compiled with the -fPIC compilation flag (such
as all the static libraries they use).
But since this compilation flag has a significant impact on performances, we
don't want to compile our programs (and the static libraries they use) with
it.

The purpose of this code is to add the -fPIC compilation flag to all the
shared libraries task generators, and to replace all the static libraries they
use by a '-fPIC' version (by copying the original task generator and adding
the compilation flag).

Since this has a significant impact on the compilation time, it can be
disabled using the NO_DOUBLE_FPIC environment variable at configure
(NO_DOUBLE_FPIC=1 waf configure), but keep in mind this has an impact at
runtime. For this reason, disabling the double compilation is not allowed in
release profile.
"""

# The list of keys to skip when copying a TaskGen stlib on declare_fpic_lib()
SKIPPED_STLIB_TGEN_COPY_KEYS = set((
    '_name', 'bld', 'env', 'features', 'idx', 'path', 'target',
    'tg_idx_count',
))

def declare_fpic_lib(ctx, pic_name, orig_lib):
    ctx_path_bak = ctx.path
    ctx.path = orig_lib.path

    # Create a new TaskGen stlib by copying the attributes of the original
    # lib TaskGen.
    # XXX: TaskGen.clone() does not work in our case because it does not
    # create a stlib TaskGen, but a generic TaskGen. Moreover, it copies some
    # attributes that should not be copied.
    orig_lib_attrs = dict(
        (key, copy.copy(value)) for key, value in orig_lib.__dict__.items()
        if key not in SKIPPED_STLIB_TGEN_COPY_KEYS
    )
    lib = ctx.stlib(target=pic_name, features=orig_lib.features,
                    env=orig_lib.env.derive(), **orig_lib_attrs)
    ctx.path = ctx_path_bak

    lib.env.append_value('CFLAGS', ['-fPIC'])


def compile_fpic(ctx):
    pic_libs = set()

    for tgen in ctx.get_all_task_gen():
        features = tgen.to_list(getattr(tgen, 'features', []))

        if not 'cshlib' in features:
            if 'c' in features or 'cxx' in features:
                tgen.env.append_value('CFLAGS', ctx.env.CNOPICFLAGS)
                tgen.env.append_value('CXXFLAGS', ctx.env.CXXNOPICFLAGS)
                tgen.env.append_value('LDFLAGS', ctx.env.LDNOPICFLAGS)
            continue

        # Shared libraries must be compiled with the -fPIC compilation flag...
        tgen.env.append_value('CFLAGS', ['-fPIC'])

        # ...such as all the libraries they use
        def process_use_pic(tgen, use_attr):
            # for all the libraries used by the shared library...
            use = tgen.to_list(getattr(tgen, use_attr, []))
            for i in range(len(use)):
                use_name = use[i]

                if use_name.endswith('.pic'):
                    # already a pic library
                    continue

                try:
                    use_tgen = ctx.get_tgen_by_name(use_name)
                except Errors.WafError:
                    # the 'use' element does not have an associatde task
                    # generator; probably an external library
                    continue

                features = use_tgen.to_list(getattr(use_tgen, 'features', []))
                if not 'cstlib' in features:
                    # the 'use' element is not a static library
                    continue

                # Replace the static library by the pic version in the shared
                # library sources
                pic_name = use_name + '.pic'
                use[i] = pic_name

                # Declare the pic static library, if not done yet
                if not use_name in pic_libs:
                    declare_fpic_lib(ctx, pic_name, use_tgen)
                    pic_libs.add(use_name)

        # Process the use and use_whole lists
        process_use_pic(tgen, 'use')
        process_use_pic(tgen, 'use_whole')


# }}}
# {{{ Patch C tasks for compression


def patch_c_tasks_for_compression(ctx):
    '''
    This function recreates the c, cprogram and cshlib task classes in order
    to add the compression of the debug sections using objcopy.
    We can't simply replace the run_str field of each class because it was
    already compiled into a 'run' method at this point.
    '''
    # pylint: disable = invalid-name
    compress_str = '${OBJCOPY} --compress-debug-sections ${TGT}'

    class c(Task):
        run_str = [c_tool.c.orig_run_str, compress_str]
        vars    = c_tool.c.vars
        ext_in  = c_tool.c.ext_in
        scan    = c_preproc.scan
    c_tool.c = c

    class cprogram(ccroot.link_task):
        run_str = [c_tool.cprogram.orig_run_str, compress_str]
        vars    = c_tool.cprogram.vars
        ext_out = c_tool.cprogram.ext_out
        inst_to = c_tool.cprogram.inst_to
    c_tool.cprogram = cprogram

    class cshlib(cprogram):
        inst_to = c_tool.cshlib.inst_to
    c_tool.cshlib = cshlib


# }}}
# {{{ Execute commands from project root

def register_get_cwd():
    '''
    Execute the compiler's commands from the project root instead of the
    project build directory.
    This is important for us because some code (for example the Z tests
    registration) relies on the value of the __FILE__ macro.
    '''
    def get_cwd(self):
        return self.env.PROJECT_ROOT

    c_tool.c.get_cwd = get_cwd
    cxx.cxx.get_cwd = get_cwd
    ccroot.link_task.get_cwd = get_cwd
    TaskGen.task_gen.get_cwd = get_cwd


# }}}
# {{{ Register global includes


def register_global_includes(self, includes):
    ''' Register global includes (that are added to all the targets). '''
    for include in Utils.to_list(includes):
        node = self.path.find_node(include)
        if node is None or not node.isdir():
            msg = 'cannot find include path `{0}` from `{1}`'
            self.fatal(msg.format(include, self.path))
        self.env.append_unique('INCLUDES', node.abspath())


# }}}
# {{{ Deploy targets / patch tasks to build targets in the source directory

class DeployTarget(Task):
    color = 'CYAN'

    @classmethod
    def keyword(cls):
        return 'Deploying'

    def __str__(self):
        node = self.outputs[0]
        return node.path_from(node.ctx.launch_node())

    def run(self):
        # Create a hardlink from source to target
        out_node = self.outputs[0]
        out_node.delete(evict=False)
        os.link(self.inputs[0].abspath(), out_node.abspath())


@TaskGen.feature('cprogram', 'cxxprogram')
@TaskGen.after_method('apply_link')
def deploy_program(self):
    # Build programs in the corresponding source directory
    assert (len(self.link_task.outputs) == 1)
    node = self.link_task.outputs[0]
    self.link_task.outputs = [node.get_src()]


@TaskGen.feature('cshlib')
@TaskGen.after_method('apply_link')
def deploy_shlib(self):
    # Build C shared library in the corresponding source directory,
    # stripping the 'lib' prefix
    assert (len(self.link_task.outputs) == 1)
    node = self.link_task.outputs[0]
    assert (node.name.startswith('lib'))
    tgt = node.parent.get_src().make_node(node.name[len('lib'):])
    self.link_task.outputs = [tgt]


@TaskGen.feature('jar')
@TaskGen.after_method('jar_files')
def deploy_jar(self):
    # Build Java jar files in the corresponding source directory
    assert (len(self.jar_task.outputs) == 1)
    node = self.jar_task.outputs[0]
    self.jar_task.outputs = [node.get_src()]


@TaskGen.feature('deploy_javac')
@TaskGen.after_method('apply_java')
def deploy_javac(self):
    src = self.outdir.make_node(self.destfile)
    tgt = self.outdir.get_src().make_node(self.destfile)
    tsk = self.create_task('DeployTarget', src=src, tgt=tgt)
    tsk.set_run_after(self.javac_task)


# }}}
# {{{ .local_vimrc.vim / syntastic configuration generation


def get_linter_flags(ctx, flags_key):
    include_flags = []
    for key in ctx.env:
        if key == 'INCLUDES' or key.startswith('INCLUDES_'):
            include_flags += ['-I' + value for value in ctx.env[key]]

    return ctx.env[flags_key] + ctx.env.CFLAGS_python2 + include_flags


def gen_local_vimrc(ctx):
    content = ""

    # Generate ALE options.
    # Escape the -D flags with double quotes, which is needed for
    # -D"index(s,c)=index__(s,c)"
    flags = get_linter_flags(ctx, 'CLANG_FLAGS')
    for i, flag in enumerate(flags):
        if flag.startswith('-D'):
            flags[i] = '-D"' + flag[2:] + '"'

    # for older versions of ALE
    content += "let g:ale_c_clang_options = '\n"
    content += "    \\ "
    content += " ".join(flags)
    content += "\n"
    content += "\\'\n"

    # for ALE 3.0+
    # https://github.com/dense-analysis/ale/issues/3299
    content += "let g:ale_c_cc_options = '\n"
    content += "    \\ "
    content += " ".join(flags)
    content += "\n"
    content += "\\'\n"

    # Bind :make to waf
    content += r"set makeprg=LC_ALL=C\ NO_WWW=1\ waf"
    content += "\n"

    # Update errorformat so that vim finds the files when compiling with :make
    content += r"set errorformat^=\%D%*\\a:\ Entering\ directory\ `%f/"
    content += ctx.bldnode.name
    content += "'\n"

    # Write file if it changed
    node = ctx.srcnode.make_node('.local_vimrc.vim')
    if not node.exists() or node.read() != content:
        node.write(content)
        ctx.msg('Writing local vimrc configuration file', node)


def gen_syntastic(ctx):
    """
    Syntastic is a vim syntax checker extension. It is not used by anybody
    anymore, but its configuration file is used by the YouCompleteMe plugin,
    that is used by some people.

    https://github.com/vim-syntastic/syntastic
    """
    def write_file(filename, what, envs):
        node = ctx.srcnode.make_node(filename)
        content = '\n'.join(envs) + '\n'
        if not node.exists() or node.read() != content:
            node.write(content)
            msg = 'Writing syntastic {0} configuration file'.format(what)
            ctx.msg(msg, node)

    write_file('.syntastic_c_config', 'C',
               get_linter_flags(ctx, 'CLANG_FLAGS'))
    write_file('.syntastic_cpp_config', 'C++',
               get_linter_flags(ctx, 'CLANGXX_FLAGS'))


# }}}
# {{{ Tags


def gen_tags(ctx):
    if ctx.cmd == 'tags':
        tags_options = ''
        tags_output  = '.tags'
    elif ctx.cmd == 'etags':
        tags_options = '-e'
        tags_output  = 'TAGS'
    else:
        return

    # Generate tags using ctags
    cmd = '{0} "{1}" "{2}"'.format(ctx.env.CTAGS_SH[0],
                                   tags_options, tags_output)
    if ctx.exec_command(cmd, stdout=None, stderr=None, cwd=ctx.srcnode):
        ctx.fatal('ctags generation failed')

    # Interrupt the build
    ctx.groups = []


class TagsClass(BuildContext):
    '''generate tags using ctags'''
    cmd = 'tags'


class EtagsClass(BuildContext):
    '''generate tags for emacs using ctags'''
    cmd = 'etags'


# }}}

# {{{ BLK


def compute_clang_extra_cflags(self, includes_field, clang_flags, cflags):
    ''' Compute clang cflags for a task generator from CFLAGS '''

    if hasattr(self, includes_field):
        return

    if not hasattr(self, 'uselib'):
        # This will also be done by waf itself on the same task generator,
        # resulting in doubled flags in the GCC arguments. It works, but is
        # not really elegant.
        # To fix that, deep copying the environment works but it has a
        # disastrous impact on performances.
        # I have not found any other solution, so keep it like that for now.
        self.process_use()
        self.propagate_uselib_vars()

    def keep_flag(flag):
        if not flag.startswith('-I') and not flag.startswith('-D'):
            return False
        return flag not in clang_flags

    cflags = self.env[cflags]
    includes = [flag for flag in cflags if keep_flag(flag)]
    setattr(self, includes_field, includes)


def compute_clang_c_env(self):
    # Compute clang extra cflags from gcc flags
    compute_clang_extra_cflags(self, 'clang_extra_cflags',
                               self.env.CLANG_FLAGS, 'CFLAGS')

    # Get cflags of the task generator
    if not 'CLANG_CFLAGS' in self.env:
        cflags = self.to_list(getattr(self, 'cflags', []))
        self.env.CLANG_CFLAGS = cflags


class Blk2c(Task):
    run_str = ['rm -f ${TGT}',
               ('${CLANG} -cc1 -x c ${CLANG_REWRITE_FLAGS} ${CLANG_CFLAGS} '
                '-rewrite-blocks ${CLANG_EXTRA_CFLAGS} '
                '${CPPPATH_ST:INCPATHS} ${SRC} -o ${TGT}')]
    ext_out = [ '.c' ]
    color = 'CYAN'

    @classmethod
    def keyword(cls):
        return 'Rewriting'


@extension('.blk')
def process_blk(self, node):
    if self.env.COMPILER_CC == 'clang':
        # clang is our C compiler -> directly compile the file
        self.create_compiled_task('c', node)
    else:
        # clang is not our C compiler -> it has to be rewritten first
        blk_c_node = node.change_ext_src('.blk.c')

        if not blk_c_node in self.env.GEN_FILES:
            self.env.GEN_FILES.add(blk_c_node)

            # Create block rewrite task.
            compute_clang_c_env(self)
            blk_task = self.create_task('Blk2c', node, blk_c_node)
            blk_task.cwd = self.env.PROJECT_ROOT
            blk_task.env.CLANG_EXTRA_CFLAGS = self.clang_extra_cflags

        # Create C compilation task for the generated C source.
        self.create_compiled_task('c', blk_c_node)


# }}}
# {{{ BLKK

class Blkk2cc(Task):
    run_str = ['rm -f ${TGT}',
               ('${CLANGXX} -cc1 -x c++ ${CLANGXX_REWRITE_FLAGS} '
                '${CLANGXX_EXTRA_CFLAGS} -rewrite-blocks '
                '${CPPPATH_ST:INCPATHS} ${SRC} -o ${TGT}')]
    ext_out = [ '.cc' ]
    color = 'CYAN'

    @classmethod
    def keyword(cls):
        return 'Rewriting'


@extension('.blkk')
def process_blkk(self, node):
    if self.env.COMPILER_CXX == 'clang++':
        # clang++ is our C++ compiler -> directly compile the file
        self.create_compiled_task('cxx', node)
    else:
        # clang++ is not our C++ compiler -> it has to be rewritten first
        blkk_cc_node = node.change_ext_src('.blkk.cc')

        if not blkk_cc_node in self.env.GEN_FILES:
            self.env.GEN_FILES.add(blkk_cc_node)

            # Compute clang extra cflags from g++ flags
            compute_clang_extra_cflags(self, 'clangxx_extra_cflags',
                                       self.env.CLANGXX_REWRITE_FLAGS,
                                       'CXXFLAGS')

            # Create block rewrite task.
            blkk_task = self.create_task('Blkk2cc', node, blkk_cc_node)
            blkk_task.cwd = self.env.PROJECT_ROOT
            blkk_task.env.CLANGXX_EXTRA_CFLAGS = self.clangxx_extra_cflags

        # Create CC compilation task for the generated c++ source.
        self.create_compiled_task('cxx', blkk_cc_node)


# }}}
# {{{ PERF


class Perf2c(Task):
    run_str = '${GPERF} --language ANSI-C --output-file ${TGT} ${SRC}'
    color   = 'BLUE'

    @classmethod
    def keyword(cls):
        return 'Generating'


@extension('.perf')
def process_perf(self, node):
    c_node = node.change_ext_src('.c')

    if not c_node in self.env.GEN_FILES:
        self.env.GEN_FILES.add(c_node)
        self.create_task('Perf2c', node, c_node)

    self.source.extend([c_node])


# }}}
# {{{ LEX

class Lex2c(Task):
    run_str = ['rm -f ${TGT}', '${FLEX_SH} ${SRC} ${TGT}']
    color   = 'BLUE'

    @classmethod
    def keyword(cls):
        return 'Generating'


@extension('.l')
def process_lex(self, node):
    c_node = node.change_ext_src('.c')

    if not c_node in self.env.GEN_FILES:
        self.env.GEN_FILES.add(c_node)
        self.create_task('Lex2c', node, c_node)

    self.source.extend([c_node])


# }}}
# {{{ FC


class Fc2c(Task):
    run_str = '${FARCHC} -c -o ${TGT} ${SRC[0].abspath()}'
    color   = 'BLUE'
    before  = ['Blk2c', 'Blkk2cc', 'ClangCheck']
    ext_out = ['.h']

    @classmethod
    def keyword(cls):
        return 'Generating'

    def scan(self):
        """ Parses the .fc file to get the dependencies. """
        node = self.inputs[0]
        lines = node.read().splitlines()
        variable_name_found = False
        deps = []

        for line in lines:
            line = line.strip()
            if len(line) > 0 and line[0] != '#':
                if variable_name_found:
                    dep_node = node.parent.find_node(line)
                    if dep_node is None:
                        err = 'cannot find `{0}` when scanning `{1}`'
                        node.ctx.fatal(err.format(line, node))
                    deps.append(dep_node)
                else:
                    variable_name_found = True

        return (deps, None)



@extension('.fc')
def process_fc(self, node):
    h_node = node.change_ext_src('.fc.c')

    if not h_node in self.env.GEN_FILES:
        self.env.GEN_FILES.add(h_node)
        farch_task = self.create_task('Fc2c', [node], h_node)
        farch_task.set_run_after(self.bld.farchc_task)


def post_farchc(ctx):
    ctx.farchc_tgen.post()
    ctx.farchc_task = ctx.farchc_tgen.link_task
    ctx.env.FARCHC = ctx.farchc_tgen.link_task.outputs[0].abspath()


# }}}
# {{{ TOKENS


class Tokens2c(Task):
    run_str = ('${TOKENS_SH} ${SRC[0].abspath()} ${TGT[0]} && ' +
               '${TOKENS_SH} ${SRC[0].abspath()} ${TGT[1]}')
    color   = 'BLUE'
    before  = ['Blk2c', 'Blkk2cc', 'ClangCheck']
    ext_out = ['.h', '.c']

    @classmethod
    def keyword(cls):
        return 'Generating'


@extension('.tokens')
def process_tokens(self, node):
    c_node = node.change_ext_src('tokens.c')
    h_node = node.change_ext_src('tokens.h')

    if not h_node in self.env.GEN_FILES:
        self.env.GEN_FILES.add(h_node)
        self.create_task('Tokens2c', [node], [c_node, h_node])

    self.source.append(c_node)


# }}}
# {{{ IOP

# IOPC options for a given sources path
class IopcOptions(object):

    def __init__(self, ctx, path=None, class_range=None, includes=None,
                 json_path=None, ts_path=None):
        self.ctx = ctx
        self.path = path or ctx.path
        self.class_range = class_range

        # Evaluate include nodes
        self.includes = set()
        if includes is not None:
            for include in Utils.to_list(includes):
                node = self.path.find_node(include)
                if node is None or not node.isdir():
                    ctx.fatal('cannot find include {0}'.format(include))
                self.includes.add(node)
        self.computed_includes = None

        # json path
        if json_path:
            self.json_node = self.path.make_node(json_path)
        else:
            self.json_node = None

        # typescript path
        if ts_path:
            self.ts_node = self.path.make_node(ts_path)
        else:
            self.ts_node = None

        # Add options in global cache
        assert not self.path in ctx.iopc_options
        ctx.iopc_options[self.path] = self

    @property
    def languages(self):
        """ Get the list of languages for iopc """
        res = 'c'
        if self.json_node:
            res += ',json'
        if self.ts_node:
            res += ',typescript'
        return res

    @property
    def class_range_option(self):
        """ Get the class-id range option for iopc """
        if self.class_range:
            return '--class-id-range={0}'.format(self.class_range)
        else:
            return ''

    @property
    def json_output_option(self):
        """ Get the json-output-path option for iopc """
        if self.json_node:
            return '--json-output-path={0}'.format(self.json_node)
        else:
            return ''

    @property
    def ts_output_option(self):
        """ Get the typescript-output-path option for iopc """
        if self.ts_node:
            return '--typescript-output-path={0}'.format(self.ts_node)
        else:
            return ''

    def get_includes_recursive(self, includes, seen_opts):
        """ Recursively get the IOP include paths for the current node """

        # Detect infinite recursions
        if self in seen_opts:
            return
        seen_opts.add(self)

        # Add includes of the current object in the resulting set
        includes.update(self.includes)

        # Recurse on the included nodes
        for node in self.includes:
            if node in self.ctx.iopc_options:
                self.ctx.iopc_options[node].get_includes_recursive(includes,
                                                                   seen_opts)

    @property
    def includes_option(self):
        """ Get the -I option for iopc """
        if self.computed_includes is None:
            includes = set()
            seen_opts = set()
            self.get_includes_recursive(includes, seen_opts)
            if len(includes):
                nodes = [node.abspath() for node in includes]
                nodes.sort()
                self.computed_includes = '-I{0}'.format(':'.join(nodes))
            else:
                self.computed_includes = ''
        return self.computed_includes


class Iop2c(Task):
    color   = 'BLUE'
    ext_out = ['.h', '.c']

    @classmethod
    def keyword(cls):
        return 'Generating'

    def get_cwd(self):
        return self.inputs[0].parent

    def scan(self):
        """ Gets the dependencies of the current IOP file.
            It uses the --depends command of iopc.
        """
        node = self.inputs[0]
        depfile = node.change_ext('.iop.d')

        # Manually redirect output to /dev/null because we don't want IOP
        # errors to be printed in double (once here, and once in build).
        # exec_command does not seem to allow dropping the output :-(...
        cmd = ('{iopc} {includes} --depends {depfile} -o {outdir} {source} '
               '> /dev/null 2>&1')
        cmd = cmd.format(iopc=self.env.IOPC,
                         includes=self.env.IOP_INCLUDES,
                         depfile=depfile.abspath(),
                         outdir=self.outputs[0].parent.abspath(),
                         source=node.abspath())
        if self.exec_command(cmd, cwd=self.get_cwd()):
            # iopc falied, run should fail too
            self.scan_failed = True
            return ([], None)

        deps = depfile.read().splitlines()
        deps = [self.bld.root.make_node(dep) for dep in deps]
        return (deps, None)

    def run(self):
        cmd = ('{iopc} --Wextra --language {languages} '
               '--c-resolve-includes --typescript-enable-backbone '
               '{includes} {class_range} {json_output} {ts_output} {source}')
        cmd = cmd.format(iopc=self.env.IOPC,
                         languages=self.env.IOP_LANGUAGES,
                         includes=self.env.IOP_INCLUDES,
                         class_range=self.env.IOP_CLASS_RANGE,
                         json_output=self.env.IOP_JSON_OUTPUT,
                         ts_output=self.env.IOP_TS_OUTPUT,
                         source=self.inputs[0].abspath())
        res = self.exec_command(cmd, cwd=self.get_cwd())
        if res and not getattr(self, 'scan_failed', False):
            self.bld.fatal("scan should have failed for %s" % self.inputs[0])
        return res


RE_IOP_PACKAGE = re.compile(r'^package (.*);$', re.MULTILINE)

def iop_get_package_path(self, node):
    """ Get the 'package path' of a IOP file.
        It opens the IOP file, parses the 'package' line, and returns a string
        where dots are replaced by slashes.
    """
    match = RE_IOP_PACKAGE.search(node.read())
    if match is None:
        self.bld.fatal('no package declaration found in %s' % node)
    return match.group(1).replace('.', '/')


@extension('.iop')
def process_iop(self, node):
    c_node = node.change_ext_src('.iop.c')

    if not c_node in self.env.GEN_FILES:
        self.env.GEN_FILES.add(c_node)

        # Get options
        if self.path in self.bld.iopc_options:
            opts = self.bld.iopc_options[self.path]
        else:
            opts = IopcOptions(self.bld, path=self.path)

        # Build list of outputs
        outputs = [c_node,
                   node.change_ext_src('.iop.h'),
                   node.change_ext_src('-tdef.iop.h'),
                   node.change_ext_src('-t.iop.h')]
        if opts.json_node or opts.ts_node:
            package_path = iop_get_package_path(self, node)
        if opts.json_node:
            json_path = package_path + '.iop.json'
            outputs.append(opts.json_node.make_node(json_path))
        if opts.ts_node:
            ts_path = package_path + '.iop.ts'
            outputs.append(opts.ts_node.make_node(ts_path))

        # Create iopc task
        task = self.create_task('Iop2c', node, outputs)
        task.bld = self.bld
        task.set_run_after(self.bld.iopc_task)

        # Set options in environment
        task.env.IOP_LANGUAGES   = opts.languages
        task.env.IOP_INCLUDES    = opts.includes_option
        task.env.IOP_CLASS_RANGE = opts.class_range_option
        task.env.IOP_JSON_OUTPUT = opts.json_output_option
        task.env.IOP_TS_OUTPUT   = opts.ts_output_option

    self.source.append(c_node)


def post_iopc(ctx):
    ctx.iopc_tgen.post()
    ctx.iopc_task = ctx.iopc_tgen.link_task
    ctx.env.IOPC = ctx.iopc_tgen.link_task.outputs[0].abspath()


# }}}
# {{{ LD


@extension('.ld')
def process_ld(self, node):
    self.env.append_value('LDFLAGS',
                          ['-Xlinker', '--version-script',
                           '-Xlinker', node.abspath()])


# }}}
# {{{ .c checks using clang


class ClangCheck(Task):
    run_str = ('${CLANG} -x c -O0 -fsyntax-only ${CLANG_FLAGS} '
               '${CLANG_CFLAGS} ${CLANG_EXTRA_CFLAGS} ${CPPPATH_ST:INCPATHS} '
               '${SRC} -o /dev/null')
    color = 'BLUE'

    @classmethod
    def keyword(cls):
        return 'Checking'


@extension('.c')
def process_c_for_check(self, node):
    # Call standard C hook
    c_task = c_tool.c_hook(self, node)

    # Do not check files in .pic task generators (they are already checked in
    # the original task generator)
    if self.name.endswith('.pic'):
        return

    # Test if checks are globally disabled...
    if not self.env.DO_CHECK:
        return

    # ...or locally disabled
    if hasattr(self, 'nocheck'):
        if isinstance(self.nocheck, bool):
            if self.nocheck:
                # Checks are disabled for this task generator
                return
        else:
            if node.path_from(self.path) in self.nocheck:
                # Checks are disabled for this node
                return

    # Do not check generated files, or files for which a check task was
    # already created
    if node in self.env.GEN_FILES or node in self.env.CHECKED_FILES:
        return

    # Create a clang check task
    self.env.CHECKED_FILES.add(node)
    compute_clang_c_env(self)
    clang_check_task = self.create_task('ClangCheck', node)
    clang_check_task.cwd = self.env.PROJECT_ROOT
    clang_check_task.env.CLANG_EXTRA_CFLAGS = self.clang_extra_cflags
    c_task.set_run_after(clang_check_task)


# }}}

# {{{ options

def options(ctx):
    # Load C/C++ compilers
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')

# }}}
# {{{ configure
# {{{ compilation profiles


def get_cflags(ctx, args):
    # TODO waf: maybe rewrite it in full-python after getting rid of make
    flags = ctx.cmd_and_log(ctx.env.CFLAGS_SH + args)
    return flags.strip().replace('"', '').split(' ')


def profile_default(ctx,
                    no_assert=False,
                    allow_no_compress=True,
                    allow_no_double_fpic=True,
                    fortify_source='-D_FORTIFY_SOURCE=2'):

    # Load C/C++ compilers
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')

    # Get compilation flags with cflags.sh
    ctx.find_program('cflags.sh', var='CFLAGS_SH',
                     path_list=[os.path.join(ctx.path.abspath(), 'Config')])

    ctx.env.CFLAGS = get_cflags(ctx, [ctx.env.COMPILER_CC])
    ctx.env.CFLAGS += [
        '-ggdb3',
        '-DWAF_MODE',
        '-fno-omit-frame-pointer',
        '-fvisibility=hidden',
    ]
    ctx.env.LINKFLAGS = [
        '-Wl,--export-dynamic',
        # Debian stretch uses --enable-new-dtags by default, which breaks
        # indirect library dependencies loading when using -rpath.
        # See https://sourceware.org/ml/binutils/2014-02/msg00031.html
        #  or https://reviews.llvm.org/D8836
        '-Xlinker', '--disable-new-dtags',
        '-Wl,--disable-new-dtags',
    ]
    ctx.env.LDFLAGS = [
        '-lpthread',
        '-ldl',
        '-lm',
        '-lrt',
    ]

    ctx.env.CXXFLAGS = get_cflags(ctx, [ctx.env.COMPILER_CXX])
    ctx.env.CXXFLAGS += [
        '-ggdb3',
        '-DWAF_MODE',
        '-D__STDC_LIMIT_MACROS',
        '-D__STDC_CONSTANT_MACROS',
        '-D__STDC_FORMAT_MACROS',
        '-fno-omit-frame-pointer',
        '-fvisibility=hidden',
    ]

    ctx.env.CLANG = ctx.find_program('clang')
    ctx.env.CLANG_FLAGS = get_cflags(ctx, ['clang'])
    ctx.env.CLANG_FLAGS += ['-DWAF_MODE']
    ctx.env.CLANG_REWRITE_FLAGS = get_cflags(ctx, ['clang', 'rewrite'])

    ctx.env.CLANGXX = ctx.find_program('clang++')
    ctx.env.CLANGXX_FLAGS = get_cflags(ctx, ['clang++'])
    ctx.env.CLANGXX_FLAGS += ['-DWAF_MODE']
    ctx.env.CLANGXX_REWRITE_FLAGS = get_cflags(ctx, ['clang++', 'rewrite'])

    if no_assert:
        ctx.env.NDEBUG = True
        ctx.env.CFLAGS += ['-DNDEBUG']
        ctx.env.CXXFLAGS += ['-DNDEBUG']

    if fortify_source is not None:
        ctx.env.CFLAGS += [fortify_source]

    # Checks
    if ctx.get_env_bool('NOCHECK'):
        ctx.env.DO_CHECK = False
        log = 'no'
    else:
        ctx.env.DO_CHECK = True
        log = 'yes'
    ctx.msg('Do checks', log)

    # Compression
    if allow_no_compress and ctx.get_env_bool('NOCOMPRESS'):
        ctx.env.DO_COMPRESS = False
        log = 'no'
    else:
        ctx.env.DO_COMPRESS = True
        log = 'yes'

        ld_help = ctx.cmd_and_log('ld --help')
        if 'compress-debug-sections' in ld_help:
            ctx.env.LDFLAGS += ['-Xlinker', '--compress-debug-sections=zlib']
        else:
            Logs.warn('Compression requested but ld do not support it')

        objcopy_help = ctx.cmd_and_log('objcopy --help')
        if 'compress-debug-sections' in objcopy_help:
            ctx.env.DO_OBJCOPY_COMPRESS = True
        else:
            Logs.warn('Compression requested but objcopy do not support it')

    ctx.msg('Do compression', log)

    # Disable double fPIC compilation for shared libraries?
    if allow_no_double_fpic and ctx.get_env_bool('NO_DOUBLE_FPIC'):
        ctx.env.DO_DOUBLE_FPIC = False
        ctx.env.CFLAGS += ['-fPIC']
        log = 'no'
    else:
        ctx.env.DO_DOUBLE_FPIC = True
        log = 'yes'
    ctx.msg('Double fPIC compilation for shared libraries', log)


def profile_debug(ctx, allow_no_double_fpic=True):
    profile_default(ctx, fortify_source=None,
                    allow_no_double_fpic=allow_no_double_fpic)

    pattern = re.compile("^-O[0-9]$")
    ctx.env.CFLAGS = [f for f in ctx.env.CFLAGS if not pattern.match(f)]

    cflags = [
        '-O0', '-Wno-uninitialized', '-fno-inline', '-fno-inline-functions',
        '-gdwarf-2',
    ]
    ctx.env.CFLAGS += cflags
    ctx.env.CXXFLAGS += cflags



def profile_release(ctx):
    profile_default(ctx, no_assert=True, allow_no_compress=False,
                    allow_no_double_fpic=False)
    ctx.env.LINKFLAGS += ['-Wl,-x', '-rdynamic']


def profile_asan(ctx):
    Options.options.check_c_compiler = 'clang'
    Options.options.check_cxx_compiler = 'clang++'

    profile_debug(ctx, allow_no_double_fpic=False)

    ctx.env.CFLAGS += ['-x', 'c']
    ctx.env.CXXFLAGS += ['-x', 'c++']
    ctx.env.LDFLAGS += ['-lstdc++']

    asan_flags = ['-fsanitize=address']
    ctx.env.CNOPICFLAGS = asan_flags
    ctx.env.CXXNOPICFLAGS = asan_flags
    ctx.env.LDNOPICFLAGS = asan_flags


def profile_tsan(ctx):
    Options.options.check_c_compiler = 'clang'
    Options.options.check_cxx_compiler = 'clang++'

    profile_debug(ctx, allow_no_double_fpic=False)

    ctx.env.CFLAGS += ['-x', 'c']
    ctx.env.CXXFLAGS += ['-x', 'c++']

    tsan_flags = ['-fsanitize=thread']
    ctx.env.CNOPICFLAGS = tsan_flags
    ctx.env.CXXNOPICFLAGS = tsan_flags
    ctx.env.LDNOPICFLAGS = tsan_flags


def profile_mem_bench(ctx):
    profile_default(ctx, no_assert=True, fortify_source=None)

    flags = ['-DMEM_BENCH']
    ctx.env.CFLAGS += flags
    ctx.env.CXXFLAGS += flags


def profile_coverage(ctx):
    # TODO waf: coverage command
    profile_debug(ctx)

    flags = ['-pg', '-fprofile-arcs', '-ftest-coverage']
    ctx.env.CFLAGS += flags
    ctx.env.CXXFLAGS += flags
    ctx.env.LDFLAGS += ['-lgcov']


PROFILES = {
    'default':   profile_default,
    'debug':     profile_debug,
    'release':   profile_release,
    'asan':      profile_asan,
    'tsan':      profile_tsan,
    'mem-bench': profile_mem_bench,
    'coverage':  profile_coverage,
}

# }}}

def configure(ctx):
    # register_global_includes
    ConfigurationContext.register_global_includes = register_global_includes

    # Load compilation profile
    ctx.env.PROFILE = os.environ.get('P', 'default')
    try:
        ctx.msg('Selecting profile', ctx.env.PROFILE)
        PROFILES[ctx.env.PROFILE](ctx)
    except KeyError:
        ctx.fatal('Profile `{0}` not found'.format(ctx.env.PROFILE))


    # Check dependencies
    ctx.find_program('objcopy', var='OBJCOPY')

    config_dir = os.path.join(ctx.path.abspath(), 'Config')
    build_dir  = os.path.join(ctx.path.abspath(), 'Build')
    ctx.find_program('_run_checks.sh', path_list=[build_dir],
                     var='RUN_CHECKS_SH')
    ctx.find_program('_tokens.sh', path_list=[config_dir], var='TOKENS_SH')
    if ctx.find_program('ctags', mandatory=False):
        ctx.find_program('ctags.sh', path_list=[build_dir], var='CTAGS_SH')


class IsConfigurationContext(ConfigurationContext):

    def execute(self):
        # Run configure
        ConfigurationContext.execute(self)

        # Ensure local vimrc and syntastic configuration files are generated
        # after the end of the configure step
        gen_local_vimrc(self)
        gen_syntastic(self)


# }}}
# {{{ build

def build(ctx):
    Logs.info('Waf: Selected profile: %s', ctx.env.PROFILE)

    ctx.env.PROJECT_ROOT = ctx.srcnode
    ctx.env.GEN_FILES = set()
    ctx.env.CHECKED_FILES = set()

    if ctx.env.DO_OBJCOPY_COMPRESS:
        patch_c_tasks_for_compression(ctx)

    register_get_cwd()

    # iopc options
    ctx.IopcOptions = IopcOptions
    ctx.iopc_options = {}

    # Register pre/post functions
    if ctx.env.NDEBUG:
        ctx.add_pre_fun(filter_out_zchk)
    if ctx.env.DO_DOUBLE_FPIC:
        ctx.add_pre_fun(compile_fpic)
    ctx.add_pre_fun(post_farchc)
    ctx.add_pre_fun(post_iopc)
    ctx.add_pre_fun(gen_tags)

# }}}
