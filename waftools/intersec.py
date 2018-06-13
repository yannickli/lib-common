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
from itertools import chain

# pylint: disable = import-error
from waflib import TaskGen, Utils

from waflib.Build import BuildContext
from waflib.Configure import ConfigurationContext
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

@TaskGen.feature('c', 'cprogram', 'cstlib')
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

        # TODO: filter STLIB_PATH by removing unused ones.
        self.env.append_value(
            'LINKFLAGS',
            list(chain.from_iterable(('-Xlinker', p.path_from(cwd))
                                     for p in lib_task.link_task.outputs)))

    # ...and close the whole archive mode
    self.env.append_value('LINKFLAGS', '-Wl,--no-whole-archive')

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

    run_str = ['rm -f ${TGT}', 'ln ${SRC} ${TGT}']
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

# {{{ BLK

def compute_clang_includes(self, includes_field, cflags):
    if hasattr(self, includes_field):
        return

    # XXX: backup the environment and restore it after because
    #      process_use/propagate_uselib_vars will also be done by waf
    #      itself on the same task generator, resulting in doubled
    #      flags in the GCC arguments otherwise.
    env_bak = self.env

    if not hasattr(self, 'uselib'):
        self.env = self.env.derive()
        self.env.detach()
        self.process_use()
        self.propagate_uselib_vars()

    cflags = self.env[cflags]
    includes = [flag for flag in cflags if flag.startswith('-I')]
    setattr(self, includes_field, includes)
    self.env = env_bak

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
    # Compute includes from gcc flags
    compute_clang_includes(self, 'clang_includes', 'CFLAGS')

    # Get cflags of the task generator
    if not 'CLANG_CFLAGS' in self.env:
        self.env.CLANG_CFLAGS = self.to_list(getattr(self, 'cflags', []))

    # Create block rewrite task.
    blk_c_node = node.change_ext_src('.blk.c')
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
    # Compute includes from g++ flags
    compute_clang_includes(self, 'clangxx_includes', 'CXXFLAGS')

    # Create block rewrite task.
    blkk_cc_node = node.change_ext_src('.blkk.cc')
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
    task = self.create_task('Perf2c', node, node.change_ext_src('.c'))
    self.source.extend(task.outputs)

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
    task = self.create_task('Lex2c', node, node.change_ext_src('.c'))
    self.source.extend(task.outputs)

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
    farch_task = self.create_task('Fc2c', [node],
                                  node.change_ext_src('.fc.h'))
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
    # Create iopc task
    c_node = node.change_ext_src('.iop.c')
    h_node = node.change_ext_src('.iop.h')
    tdef_h_node = node.change_ext_src('-tdef.iop.h')
    t_h_node = node.change_ext_src('-t.iop.h')
    task = self.create_task('Iop2c', node,
                            [c_node, h_node, tdef_h_node, t_h_node])
    task.bld = self.bld
    task.set_run_after(self.bld.iopc_task)
    self.source.append(c_node)

    # Handle iopc options
    if self.path in self.bld.iopc_options:
        opts = self.bld.iopc_options[self.path]
        task.env.IOP_CLASS_RANGE = opts.class_range_option
        task.env.IOP_INCLUDES = opts.includes_option
    else:
        task.env.IOP_CLASS_RANGE = ''
        task.env.IOP_INCLUDES = ''


# }}}

# {{{ options

def options(ctx):
    # Load C/C++ compilers
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')

# }}}
# {{{ configure


def get_cflags(ctx, args):
    # TODO: maybe rewrite it in full-python after getting rid of make
    flags = ctx.cmd_and_log(ctx.env.CFLAGS_SH + args)
    return flags.strip().replace('"', '').split(' ')


def configure(ctx):
    # Load C/C++ compilers
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')

    # register_global_includes
    ConfigurationContext.register_global_includes = register_global_includes

    # {{{ Compilation flags

    ctx.find_program('cflags.sh', mandatory=True, var='CFLAGS_SH',
                     path_list=[os.path.join(ctx.path.abspath(), 'Config')])

    ctx.env.CFLAGS = get_cflags(ctx, [ctx.env.COMPILER_CC])
    ctx.env.CFLAGS += [
        '-DWAF_MODE',
        '-fPIC', # TODO: understand this
    ]
    ctx.env.LINKFLAGS = [
        '-Wl,--export-dynamic',
    ]
    ctx.env.LDFLAGS = [
        '-lpthread',
        '-ldl',
        '-lm',
    ]

    ctx.env.CXXFLAGS = get_cflags(ctx, [ctx.env.COMPILER_CXX])
    ctx.env.CXXFLAGS += ['-DWAF_MODE']

    ctx.env.CLANG = ctx.find_program('clang')
    ctx.env.CLANG_FLAGS = get_cflags(ctx, ['clang'])
    ctx.env.CLANG_FLAGS += ['-DWAF_MODE']
    ctx.env.CLANG_REWRITE_FLAGS = get_cflags(ctx, ['clang', 'rewrite'])

    ctx.env.CLANGXX = ctx.find_program('clang++')
    ctx.env.CLANGXX_FLAGS = get_cflags(ctx, ['clang++'])
    ctx.env.CLANGXX_FLAGS += ['-DWAF_MODE']
    ctx.env.CLANGXX_REWRITE_FLAGS = get_cflags(ctx, ['clang++', 'rewrite'])

    # }}}


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
    ctx.env.PROJECT_ROOT = ctx.srcnode

    register_get_cwd()

    # iopc options
    ctx.IopcOptions = IopcOptions
    ctx.iopc_options = {}

    # Register pre/post functions
    ctx.add_post_fun(run_checks)

# }}}
