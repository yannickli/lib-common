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
import errno
import copy
from itertools import chain

# pylint: disable = import-error
from waflib import TaskGen, Utils

from waflib.Build import BuildContext
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
        if uw in self.use:
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
# {{{ Add global includes


@TaskGen.feature('c', 'cprogram', 'cstlib')
@TaskGen.after_method('process_use')
def add_includes(self):
    includes = self.to_list(getattr(self, 'includes', []))
    includes.extend(self.bld.env.GLOBAL_INCLUDES)
    self.includes = includes


def add_global_includes(self, includes):
    ''' Add global includes (that are added to all the targets). '''
    for include in Utils.to_list(includes):
        node = self.path.find_node(include)
        if node is None or not node.isdir():
            msg = 'cannot find include path `{0}` from `{1}`'
            self.fatal(msg.format(include, self.path))
        self.env.GLOBAL_INCLUDES.append(node)

# }}}
# {{{ Deploy targets

def deploy_targets(ctx):
    """ Deploy the targets (using hard links) in the source directories once
        the build is done.
    """
    for tgen in ctx.get_all_task_gen():
        # Deploy only posted targets
        if not getattr(tgen, 'posted', False):
            continue

        # Deploy only C programs and shared libraries
        features = getattr(tgen, 'features', [])
        is_program = 'cprogram' in features or 'cxxprogram' in features
        is_cshlib = 'cshlib' in features
        if not is_program and not is_cshlib:
            continue

        node = tgen.link_task.outputs[0]

        if is_program:
            # Deploy C programs in the corresponding source directory
            dst = node.get_src().abspath()
        else:
            # Deploy C shared library in the corresponding source
            # directory, stripping the 'lib' prefix
            assert (node.name.startswith('lib'))
            name = node.name[len('lib'):]
            dst = os.path.join(node.parent.get_src().abspath(), name)

        # Remove possibly existing file, and create hard link
        try:
            os.remove(dst)
        except OSError as exn:
            if exn.errno != errno.ENOENT:
                raise
        os.link(node.abspath(), dst)

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

# {{{ BLK

def compute_clang_includes(self, includes_field, cflags):
    if not hasattr(self, includes_field):
        if not hasattr(self, 'uselib'):
            # FIXME: this will also be done by waf itself on the same task
            #        generator, resulting in doubled flags in the GCC
            #        arguments. A solution would be do deepcopy "self", but it
            #        fails...
            self.process_use()
            self.propagate_uselib_vars()
        cflags = self.env[cflags]
        includes = [flag for flag in cflags if flag.startswith('-I')]
        setattr(self, includes_field, includes)

class Blk2c(Task):
    run_str = ['rm -f ${TGT}',
               ('${CLANG} ${CLANG_REWRITE_FLAGS} ${CLANG_INCLUDES} '
                '${CPPPATH_ST:INCPATHS} ${SRC} -o ${TGT}')]
    ext_out = [ '.c' ]
    color = 'CYAN'

    @classmethod
    def keyword(cls):
        return 'Rewriting'


@extension('.blk')
def process_blk(self, node):
    # Compute includes from gcc flags
    compute_clang_includes(self, 'clang_includes', 'CFLAGS')

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
               ('${CLANGXX} ${CLANGXX_REWRITE_FLAGS} ${CLANG_INCLUDES} '
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
    farch_task.set_run_after(self.env.FARCHC_TASK)

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
    task.set_run_after(self.env.IOPC_TASK)
    self.source.append(c_node)

    # Handle iopc options
    if self.path in self.bld.iopc_options:
        options = self.bld.iopc_options[self.path]
        task.env.IOP_CLASS_RANGE = options.class_range_option
        task.env.IOP_INCLUDES = options.includes_option
    else:
        task.env.IOP_CLASS_RANGE = ''
        task.env.IOP_INCLUDES = ''


# }}}


def register(ctx):
    ctx.env.PROJECT_ROOT = ctx.root.find_node(ctx.top_dir)

    register_get_cwd()

    BuildContext.add_global_includes = add_global_includes
    ctx.env.GLOBAL_INCLUDES = []

    ctx.IopcOptions = IopcOptions
    ctx.iopc_options = {}

    ctx.add_post_fun(deploy_targets)
    ctx.add_post_fun(run_checks)
