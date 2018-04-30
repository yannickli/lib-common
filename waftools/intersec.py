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
from itertools import chain

# pylint: disable = import-error
import waflib.TaskGen as TaskGen

from waflib.Build import BuildContext
from waflib.Task import Task
from waflib.TaskGen import feature, extension
from waflib.Tools import c
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
    self.use = self.to_list(self.use)
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

'''
The purpose of this section is to execute the compiler's commands from the
project root instead of the project build directory.
This is important for us because some code (for example the Z tests
registration) relies on the value of the __FILE__ macro.
'''

def register_get_cwd():
    def get_cwd(self):
        return self.env.PROJECT_ROOT

    c.c.get_cwd = get_cwd
    ccroot.link_task.get_cwd = get_cwd
    TaskGen.task_gen.get_cwd = get_cwd


@TaskGen.feature('c', 'cprogram', 'cstlib')
@TaskGen.after_method('process_use')
def include_source_dir(self):
    # Always include '.' in the tasks includes
    includes = self.to_list(getattr(self, 'includes', []))
    if not '.' in includes:
        includes.append('.')
        self.includes = includes


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
        is_cprogram = 'cprogram' in features
        is_cshlib = 'cshlib' in features
        if not is_cprogram and not is_cshlib:
            continue

        node = tgen.link_task.outputs[0]

        if is_cprogram:
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
        cmd = '{0} {1}'.format(ctx.env.RUN_CHECKS_SH[0],
                               ctx.launch_node().path_from(ctx.path))
        if ctx.exec_command(cmd, stdout=None, stderr=None):
            ctx.fatal('')

class CheckClass(BuildContext):
    '''run tests'''
    cmd = 'check'

# }}}

# {{{ BLK

class Blk2c(Task):
    # INCPATHS: includes paths are the same as for C compilation.
    run_str = ('${CLANG} ${CLANG_REWRITE_FLAGS} ${CPPPATH_ST:INCPATHS} '
               '${SRC} -o ${TGT}')
    ext_out = [ '.c' ]
    color = 'CYAN'

    @classmethod
    def keyword(cls):
        return 'Rewriting'


@extension('.blk')
@feature('blk')
def process_blk(self, node):
    # Create block rewrite task.
    blk_c_node = node.change_ext('.blk.c')
    blk_task = self.create_task('Blk2c', node, blk_c_node)
    blk_task.cwd = self.env.PROJECT_ROOT

    # Create C compilation task for the generated C source.
    self.create_compiled_task('c', blk_c_node)

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
    task = self.create_task('Perf2c', node, node.change_ext('.c'))
    self.source.extend(task.outputs)

# }}}
# {{{ LEX

class Lex2c(Task):
    run_str = '${FLEX_SH} ${SRC} ${TGT}'
    color   = 'BLUE'

    @classmethod
    def keyword(cls):
        return 'Generating'

@extension('.l')
def process_lex(self, node):
    task = self.create_task('Lex2c', node, node.change_ext('.c'))
    self.source.extend(task.outputs)

# }}}
# {{{ FC

class Fc2c(Task):
    run_str = '${FARCHC} -c -o ${TGT} ${SRC[0].abspath()}'
    color   = 'BLUE'

    @classmethod
    def keyword(cls):
        return 'Generating'


@extension('.fc')
def process_fc(self, node):
    farch_task = self.create_task('Fc2c', [node],
                                  node.change_ext('.fc.h'))
    farch_task.set_run_after(self.env.FARCHC_TASK)

# }}}
# {{{ IOP

class Iop2c(Task):
    run_str = ('${IOPC} --Wextra --language c ' +
               '${IOP_INCLUDES} ' +
               '${IOP_CLASS_RANGE} ' +
               '-o ${TGT[0].parent.abspath()} ' +
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
        depfile = node.change_ext('.iop.d').get_bld()

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
    c_node = node.change_ext('.iop.c')
    h_node = node.change_ext('.iop.h')
    tdef_h_node = node.change_ext('-tdef.iop.h')
    t_h_node = node.change_ext('-t.iop.h')
    task = self.create_task('Iop2c', node,
                            [c_node, h_node, tdef_h_node, t_h_node])
    task.bld = self.bld
    task.set_run_after(self.env.IOPC_TASK)
    self.source.append(c_node)

    # class id range
    if hasattr(self, 'iop_class_range'):
        class_range = self.iop_class_range
        task.env.IOP_CLASS_RANGE = '--class-id-range={0}'.format(class_range)
    else:
        task.env.IOP_CLASS_RANGE = ''

    # includes
    if hasattr(self, 'iop_includes'):
        task.env.IOP_INCLUDES = '-I{0}'.format(self.iop_includes)
    else:
        task.env.IOP_INCLUDES = ''

# }}}


def register(ctx):
    ctx.env.PROJECT_ROOT = ctx.path
    register_get_cwd()
    ctx.add_post_fun(deploy_targets)
    ctx.add_post_fun(run_checks)
