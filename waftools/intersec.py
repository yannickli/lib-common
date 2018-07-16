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

import copy
import os
import re
from itertools import chain

# pylint: disable = import-error
from waflib import TaskGen, Utils, Context, Errors, Options, Logs

from waflib.Build import BuildContext
from waflib.Configure import ConfigurationContext, conf
from waflib.Node import Node
from waflib.Task import Task
from waflib.TaskGen import extension
from waflib.Tools import c, cxx
from waflib.Tools import ccroot
# pylint: enable = import-error


# {{{ depends_on

@TaskGen.feature('*')
@TaskGen.before_method('process_rule')
def post_deps(self):
    deps = getattr(self, 'depends_on', [])
    for name in self.to_list(deps):
        other = self.bld.get_tgen_by_name(name)
        other.post()

# }}}
# {{{ use_whole

# These functions implement the use_whole attribute, allowing to link a
# library with -whole-archive

@TaskGen.feature('cprogram', 'cstlib', 'cshlib')
@TaskGen.before_method('process_rule')
def prepare_whole(self):
    use_whole = self.to_list(getattr(self, 'use_whole', []))
    if not use_whole:
        return

    # Add the 'use_whole' elements in the 'use' list, so that waf considers it
    # for paths, includes, ...
    self.use = copy.copy(self.to_list(self.use))
    for uw in use_whole:
        if  not getattr(self, 'no_use_whole_error', False) \
        and uw in self.use:
            self.bld.fatal(('`{0}` from `use_whole` of target `{1}` is '
                            'already in attribute `use`, you may remove it')
                           .format(uw, self.target))
        self.use.append(uw)

@TaskGen.feature('cprogram', 'cstlib', 'cshlib')
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

        # TODO: filter STLIB_PATH by removing unused ones.
        self.env.append_value(
            'LINKFLAGS',
            list(chain.from_iterable(('-Xlinker', p.path_from(cwd))
                                     for p in lib_task.link_task.outputs)))

    # ...and close the whole archive mode
    self.env.append_value('LINKFLAGS', '-Wl,--no-whole-archive')

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
"""

def add_fpic_flag(tgen):
    cflags = tgen.to_list(getattr(tgen, 'cflags', []))
    cflags.append('-fPIC')
    tgen.cflags = cflags


def declare_fpic_lib(ctx, pic_name, orig_lib):
    orig_source     = orig_lib.to_list(getattr(orig_lib, 'source',     []))
    orig_use        = orig_lib.to_list(getattr(orig_lib, 'use',        []))
    orig_depends_on = orig_lib.to_list(getattr(orig_lib, 'depends_on', []))
    orig_cflags     = orig_lib.to_list(getattr(orig_lib, 'cflags',     []))
    orig_includes   = orig_lib.to_list(getattr(orig_lib, 'includes',   []))

    lib = ctx.stlib(target=pic_name,
                    features=orig_lib.features,
                    source=list(orig_source),
                    cflags=list(orig_cflags),
                    use=orig_use,
                    path=orig_lib.path,
                    depends_on=orig_depends_on,
                    includes=orig_includes)

    add_fpic_flag(lib)


def compile_fpic(ctx):
    pic_libs = set()

    for tgen in ctx.get_all_task_gen():
        features = tgen.to_list(getattr(tgen, 'features', []))
        if not 'cshlib' in features:
            continue

        # Shared libraries must be compiled with the -fPIC compilation flag...
        add_fpic_flag(tgen)

        # ...such as all the libraries they use
        def process_use_pic(tgen, use_attr):
            # for all the libraries used by the shared library...
            use = tgen.to_list(getattr(tgen, use_attr, []))
            for i in xrange(len(use)):
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

    c.c.get_cwd = get_cwd
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
# {{{ Deploy targets

class DeployTarget(Task):

    run_str = 'ln -f ${SRC} ${TGT}'
    color = 'CYAN'

    @classmethod
    def keyword(cls):
        return 'Deploying'

    def __str__(self):
        node = self.outputs[0]
        return node.path_from(node.ctx.launch_node())


@TaskGen.feature('cprogram', 'cxxprogram')
@TaskGen.after_method('apply_link')
def deploy_program(self):
    # Deploy programs in the corresponding source directory
    node = self.link_task.outputs[0]
    self.create_task('DeployTarget', src=node, tgt=node.get_src())


@TaskGen.feature('cshlib')
@TaskGen.after_method('apply_link')
def deploy_shlib(self):
    # Deploy C shared library in the corresponding source directory,
    # stripping the 'lib' prefix
    node = self.link_task.outputs[0]
    assert (node.name.startswith('lib'))
    tgt = node.parent.get_src().make_node(node.name[len('lib'):])
    self.create_task('DeployTarget', src=node, tgt=tgt)


@TaskGen.feature('jar')
@TaskGen.after_method('jar_files')
def deploy_jar(self):
    # Deploy Java jar files in the corresponding source directory
    node = self.jar_task.outputs[0]
    tsk = self.create_task('DeployTarget', src=node, tgt=node.get_src())
    tsk.set_run_after(self.jar_task)


@TaskGen.feature('deploy_javac')
@TaskGen.after_method('apply_java')
def deploy_javac(self):
    src = self.outdir.make_node(self.destfile)
    tgt = self.outdir.get_src().make_node(self.destfile)
    tsk = self.create_task('DeployTarget', src=src, tgt=tgt)
    tsk.set_run_after(self.javac_task)


# }}}
# {{{ Run checks

def run_checks(ctx):
    if ctx.cmd == 'check':
        path = ctx.launch_node().path_from(ctx.env.PROJECT_ROOT)
        cmd = '{0} {1}'.format(ctx.env.RUN_CHECKS_SH[0], path)
        if ctx.exec_command(cmd, stdout=None, stderr=None):
            ctx.fatal('')

class CheckClass(BuildContext):
    '''run tests'''
    cmd = 'check'

# }}}
# {{{ Node::change_ext_src method

''' Declares the method Node.change_ext_src, which is similar to
    Node.change_ext, excepts it makes a node in the source directory (instead
    of the build directory).
'''

def node_change_ext_src(self, ext):
    name = self.name

    k = name.rfind('.')
    if k >= 0:
        name = name[:k] + ext
    else:
        name = name + ext

    return self.parent.make_node(name)


Node.change_ext_src = node_change_ext_src

# }}}
# {{{ syntastic/ale


def get_linter_flags(ctx, flags_key):
    include_flags = []
    for key in ctx.env:
        if key == 'INCLUDES' or key.startswith('INCLUDES_'):
            include_flags += ['-I' + value for value in ctx.env[key]]

    return ctx.env[flags_key] + ctx.env.CFLAGS_python2 + include_flags


def gen_syntastic(ctx):
    def write_file(filename, what, envs):
        node = ctx.srcnode.make_node(filename)
        content = '\n'.join(envs)
        node.write(content + '\n')
        msg = 'Writing syntastic {0} configuration file'.format(what)
        ctx.msg(msg, node)

    write_file('.syntastic_c_config', 'C',
               get_linter_flags(ctx, 'CLANG_FLAGS'))
    write_file('.syntastic_cpp_config', 'C++',
               get_linter_flags(ctx, 'CLANGXX_FLAGS'))


def gen_ale(ctx):
    flags = get_linter_flags(ctx, 'CLANG_FLAGS')

    content  = "let g:ale_c_clang_options = '\n"
    content += "    \\ "
    content += " ".join(flags)
    content += "\n"
    content += "\\'\n"

    node = ctx.srcnode.make_node('.local_vimrc.vim')
    node.write(content)
    ctx.msg('Writing ale configuration file', node)


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


def compute_clang_includes(self, includes_field, cflags):
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

    cflags = self.env[cflags]
    includes = [flag for flag in cflags if flag.startswith('-I')]
    setattr(self, includes_field, includes)


class Blk2c(Task):
    run_str = ['rm -f ${TGT}',
               ('${CLANG} -cc1 -x c ${CLANG_REWRITE_FLAGS} ${CLANG_CFLAGS} '
                '-rewrite-blocks ${CLANG_INCLUDES} ${CPPPATH_ST:INCPATHS} '
                '${SRC} -o ${TGT}')]
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

            # Compute includes from gcc flags
            compute_clang_includes(self, 'clang_includes', 'CFLAGS')

            # Get cflags of the task generator
            if not 'CLANG_CFLAGS' in self.env:
                cflags = list(self.to_list(getattr(self, 'cflags', [])))
                if '-fPIC' in cflags:
                    cflags = list(cflags)
                    cflags.remove('-fPIC')
                self.env.CLANG_CFLAGS = cflags

            # Create block rewrite task.
            blk_task = self.create_task('Blk2c', node, blk_c_node)
            blk_task.cwd = self.env.PROJECT_ROOT
            blk_task.env.CLANG_INCLUDES = self.clang_includes

        # Create C compilation task for the generated C source.
        self.create_compiled_task('c', blk_c_node)


# }}}
# {{{ BLKK

class Blkk2cc(Task):
    run_str = ['rm -f ${TGT}',
               ('${CLANGXX} -cc1 -x c++ ${CLANGXX_REWRITE_FLAGS} '
                '${CLANG_INCLUDES} -rewrite-blocks '
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

            # Compute includes from g++ flags
            compute_clang_includes(self, 'clangxx_includes', 'CXXFLAGS')

            # Create block rewrite task.
            blkk_task = self.create_task('Blkk2cc', node, blkk_cc_node)
            blkk_task.cwd = self.env.PROJECT_ROOT
            blkk_task.env.CLANG_INCLUDES = self.clangxx_includes

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
    ext_out = ['.h']

    @classmethod
    def keyword(cls):
        return 'Generating'


@extension('.fc')
def process_fc(self, node):
    h_node = node.change_ext_src('.fc.h')

    if not h_node in self.env.GEN_FILES:
        self.env.GEN_FILES.add(h_node)
        farch_task = self.create_task('Fc2c', [node], h_node)
        farch_task.set_run_after(self.bld.farchc_task)


# }}}
# {{{ TOKENS


class Tokens2c(Task):
    run_str = ('${TOKENS_SH} ${SRC[0].abspath()} ${TGT[0]} && ' +
               '${TOKENS_SH} ${SRC[0].abspath()} ${TGT[1]}')
    color   = 'BLUE'
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

    def __init__(self, ctx, class_range=None, includes=None):
        self.ctx = ctx
        self.path = ctx.path
        self.class_range = class_range

        # Evaluate include nodes
        self.includes = set()
        if includes is not None:
            for include in Utils.to_list(includes):
                node = ctx.path.find_node(include)
                if node is None or not node.isdir():
                    ctx.fatal('cannot find include {0}'.format(include))
                self.includes.add(node)
        self.computed_includes = None

        # Add options in global cache
        assert not ctx.path in ctx.iopc_options
        ctx.iopc_options[ctx.path] = self

    @property
    def class_range_option(self):
        """ Get the class-id range option for iopc """
        if self.class_range:
            return '--class-id-range={0}'.format(self.class_range)
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
    run_str = ('${IOPC} --Wextra --language c --c-resolve-includes ' +
               '${IOP_INCLUDES} ' +
               '${IOP_CLASS_RANGE} ' +
               '${SRC[0].abspath()}')
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

        cmd = '{0} {1} --depends {2} -o {3} {4}'
        cmd = cmd.format(self.env.IOPC,
                         self.env.IOP_INCLUDES,
                         depfile.abspath(),
                         self.outputs[0].parent.abspath(),
                         node.abspath())
        if self.exec_command(cmd, cwd=self.get_cwd()):
            # iopc falied, run should fail too
            return ([], None)

        pfx = self.bld.path.abspath() + '/'
        deps = depfile.read().splitlines()
        deps = [self.bld.path.make_node(dep[len(pfx):]) for dep in deps]

        return (deps, None)


@extension('.iop')
def process_iop(self, node):
    c_node = node.change_ext_src('.iop.c')

    if not c_node in self.env.GEN_FILES:
        self.env.GEN_FILES.add(c_node)

        h_node = node.change_ext_src('.iop.h')
        tdef_h_node = node.change_ext_src('-tdef.iop.h')
        t_h_node = node.change_ext_src('-t.iop.h')

        # Create iopc task
        task = self.create_task('Iop2c', node,
                                [c_node, h_node, tdef_h_node, t_h_node])
        task.bld = self.bld
        task.set_run_after(self.bld.iopc_task)

        # Handle iopc options
        if self.path in self.bld.iopc_options:
            opts = self.bld.iopc_options[self.path]
            task.env.IOP_CLASS_RANGE = opts.class_range_option
            task.env.IOP_INCLUDES = opts.includes_option
        else:
            task.env.IOP_CLASS_RANGE = ''
            task.env.IOP_INCLUDES = ''

    self.source.append(c_node)


# }}}
# {{{ LD


@extension('.ld')
def process_ld(self, node):
    self.env.append_value('LDFLAGS',
                          ['-Xlinker', '--version-script',
                           '-Xlinker', node.abspath()])


# }}}
# {{{ JAVA


def check_class_from_env_var(ctx, var, binpath, classname):
    if var not in ctx.environ:
        ctx.fatal('missing %s environment variable', var)

    cmd = [ctx.environ[var] + binpath, 'classpath']
    try:
        classpath = ctx.cmd_and_log(cmd, output=Context.STDOUT)
    except Errors.WafError as e:
        ctx.fatal('cmd `%s` failed: %s' % (' '.join(cmd), e))

    res = ctx.check_java_class(classname, with_classpath=classpath)
    if res == 0:
        return classpath
    else:
        ctx.fatal('cannot find classes with provided %s env variable' % (var))


@conf
def check_hadoop(ctx):
    classpath = check_class_from_env_var(ctx, 'HADOOP_HOME', '/bin/hadoop',
                                         'org.apache.hadoop.io.Text')
    ctx.env.HADOOP_CLASSPATH = classpath


@conf
def check_hbase(ctx):
    classpath = check_class_from_env_var(ctx, 'HBASE_LIB_DIR',
                                         '/../bin/hbase',
                                         'org.apache.hadoop.hbase.KeyValue')
    ctx.env.HBASE_CLASSPATH = classpath


class ClassToHeaderFile(Task):
    run_str = ['javah -cp ${OUTDIR} -jni -o ${TGT} ${CLASSNAME}']
    color = 'BLUE'
    ext_out = [ '.h' ]

    @classmethod
    def keyword(cls):
        return 'Generating'

    def __str__(self):
        node = self.outputs[0]
        return node.path_from(node.ctx.launch_node())


@TaskGen.feature('javah')
@TaskGen.after_method('apply_java')
def generate_java_header_file(self):
    # Make a .h file from a given class
    task1 = self.create_task('ClassToHeaderFile')
    task1.set_outputs(self.path.make_node(self.header_file))
    task1.env.CLASSNAME = self.javah_class
    task1.set_run_after(self.javac_task)


# }}}

# {{{ options

def options(ctx):
    # Load C/C++ compilers
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')

    # Profile option
    default_profile = os.environ.get('P', 'default')
    gr = ctx.get_option_group('configure options')
    h = 'Compilation profile ({0}) [default: \'{1}\']'
    h = h.format(', '.join(PROFILES.keys()), default_profile)
    gr.add_option('-P', '--PROFILE', action='store', dest='PROFILE',
                  type='string', default=default_profile, help=h)

# }}}
# {{{ configure
# {{{ compilation profiles


def get_cflags(ctx, args):
    # TODO: maybe rewrite it in full-python after getting rid of make
    flags = ctx.cmd_and_log(ctx.env.CFLAGS_SH + args)
    return flags.strip().replace('"', '').split(' ')


def profile_default(ctx, no_assert=False,
                    fortify_source='-D_FORTIFY_SOURCE=2'):

    # TODO: NOCOMPRESS (well, compress)

    # Load C/C++ compilers
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')

    # Get compilation flags with cflags.sh
    ctx.find_program('cflags.sh', mandatory=True, var='CFLAGS_SH',
                     path_list=[os.path.join(ctx.path.abspath(), 'Config')])

    ctx.env.CFLAGS = get_cflags(ctx, [ctx.env.COMPILER_CC])
    ctx.env.CFLAGS += [
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
    ]

    ctx.env.CXXFLAGS = get_cflags(ctx, [ctx.env.COMPILER_CXX])
    ctx.env.CXXFLAGS += [
        '-DWAF_MODE',
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
        ctx.env.CFLAGS += ['-DNDEBUG']
        ctx.env.CXXFLAGS += ['-DNDEBUG']

    if fortify_source is not None:
        ctx.env.CFLAGS += [fortify_source]


def profile_debug(ctx):
    profile_default(ctx, fortify_source=None)

    pattern = re.compile("^-O[0-9]$")
    ctx.env.CFLAGS = [f for f in ctx.env.CFLAGS if not pattern.match(f)]

    cflags = [
        '-O0', '-Wno-uninitialized', '-fno-inline', '-fno-inline-functions',
        '-g3', '-gdwarf-2',
    ]
    ctx.env.CFLAGS += cflags
    ctx.env.CXXFLAGS += cflags



def profile_release(ctx):
    profile_default(ctx, no_assert=True)
    ctx.env.LINKFLAGS += ['-Wl,-x', '-rdynamic']


def profile_asan(ctx):
    Options.options.check_c_compiler = 'clang'
    Options.options.check_cxx_compiler = 'clang++'

    profile_debug(ctx)

    flags = ['-fsanitize=address']
    ctx.env.CFLAGS += flags + ['-x', 'c']
    ctx.env.CXXFLAGS += flags + ['-x', 'cxx']
    ctx.env.LDFLAGS += flags + ['-lstdc++']


def profile_tsan(ctx):
    Options.options.check_c_compiler = 'clang'
    Options.options.check_cxx_compiler = 'clang++'

    profile_debug(ctx)

    flags = ['-fsanitize=thread']
    ctx.env.CFLAGS += flags + ['-x', 'c']
    ctx.env.CXXFLAGS += flags + ['-x', 'cxx']
    ctx.env.LDFLAGS += flags


def profile_mem_bench(ctx):
    profile_default(ctx, no_assert=True, fortify_source=None)

    flags = ['-DMEM_BENCH']
    ctx.env.CFLAGS += flags
    ctx.env.CXXFLAGS += flags


def profile_coverage(ctx):
    # TODO: coverage command
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
    ctx.env.PROFILE = Options.options.PROFILE
    try:
        ctx.msg('Selecting profile', ctx.env.PROFILE)
        PROFILES[ctx.env.PROFILE](ctx)
    except KeyError:
        ctx.fatal('Profile `{0}` not found'.format(ctx.env.PROFILE))


    # Check dependencies
    config_dir = os.path.join(ctx.path.abspath(), 'Config')
    build_dir  = os.path.join(ctx.path.abspath(), 'Build')
    ctx.find_program('_run_checks.sh', path_list=[build_dir], mandatory=True,
                     var='RUN_CHECKS_SH')
    ctx.find_program('_tokens.sh', path_list=[config_dir], mandatory=True,
                     var='TOKENS_SH')
    if ctx.find_program('ctags', mandatory=False):
        ctx.find_program('ctags.sh', path_list=[build_dir], mandatory=True,
                         var='CTAGS_SH')


class IsConfigurationContext(ConfigurationContext):

    def execute(self):
        # Run configure
        ConfigurationContext.execute(self)

        # Ensure syntastic/ale are done after the end of the configure step
        gen_syntastic(self)
        gen_ale(self)


# }}}
# {{{ build

def build(ctx):
    Logs.info('Waf: Selected profile: %s', ctx.env.PROFILE)

    ctx.env.PROJECT_ROOT = ctx.srcnode
    ctx.env.GEN_FILES = set()

    register_get_cwd()

    # iopc options
    ctx.IopcOptions = IopcOptions
    ctx.iopc_options = {}

    # Register pre/post functions
    ctx.add_pre_fun(compile_fpic)
    ctx.add_pre_fun(gen_tags)
    ctx.add_post_fun(run_checks)

# }}}
