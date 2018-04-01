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

from itertools import chain

import waflib.TaskGen as TaskGen

from waflib.Task import Task
from waflib.TaskGen import feature, extension


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
                          '-Wl,-Bstatic,--as-needed,--whole-archive')

    for name in use_whole:
        # ...add the whole archive libraries...
        lib_task = self.bld.get_tgen_by_name(name)
        self.env.STLIB.remove(name)

        # TODO: filter STLIB_PATH by removing unused ones.
        self.env.append_value(
            'LINKFLAGS',
            list(chain.from_iterable(('-Xlinker', p.relpath())
                                     for p in lib_task.link_task.outputs)))

    # ...and close the whole archive mode
    self.env.append_value('LINKFLAGS', '-Wl,--no-whole-archive')

# }}}

# {{{ BLK

class Blk2c(Task):
    # INCPATHS: includes paths are the same as for C compilation.
    run_str = ('${CLANG} ${CLANG_REWRITE_FLAGS} ${CPPPATH_ST:INCPATHS} '
               '${SRC} -o ${TGT}')
    ext_out = [ '.c' ]
    color = 'YELLOW'

@extension('.blk')
@feature('blk')
def process_blk(self, node):
    # TODO: Should probably be done somewhere else. It computes env.INCPATHS.
    self.process_use()
    self.apply_incpaths()

    # Create block rewrite task.
    blk_c_node = node.change_ext('.blk.c')
    self.create_task('Blk2c', node, blk_c_node)

    # Create C compilation task for the generated C source.
    self.create_compiled_task('c', blk_c_node)

# }}}
# {{{ PERF

class Perf2c(Task):
    run_str = '${GPERF} --language ANSI-C --output-file ${TGT} ${SRC}'
    color   = 'BLUE'

@extension('.perf')
def process_perf(self, node):
    task = self.create_task('Perf2c', node, node.change_ext('.c'))
    self.source.extend(task.outputs)

# }}}
# {{{ LEX

class Lex2c(Task):
    run_str = '${FLEX_SH} ${SRC} ${TGT}'
    color   = 'PURPLE'

@extension('.l')
def process_lex(self, node):
    task = self.create_task('Lex2c', node, node.change_ext('.c'))
    self.source.extend(task.outputs)

# }}}
# {{{ FC

class Fc2c(Task):
    run_str = '${FARCHC} -c -o ${TGT} ${SRC[0].abspath()}'
    color   = 'PINK'

@extension('.fc')
def process_fc(self, node):
    farch_task = self.create_task('Fc2c', [node],
                                  node.change_ext('.fc.h'))
    farch_task.set_run_after(self.env.FARCHC_TASK)

# }}}
# {{{ IOP

class Iop2c(Task):
    # TODO: handle depfiles
    # TODO: handle class ids range
    run_str = ('${IOPC} --Wextra -l c ' +
               '-I .. ' + # TODO: properly handle include path
               '-o ${TGT[0].parent.abspath()} ' +
               '${SRC[0].abspath()}')
    color   = 'BLUE'
    ext_out = ['.h', '.c']


@extension('.iop')
def process_iop(self, node):
    c_node = node.change_ext('.iop.c')
    h_node = node.change_ext('.iop.h')
    tdef_h_node = node.change_ext('-tdef.iop.h')
    t_h_node = node.change_ext('-t.iop.h')

    task = self.create_task('Iop2c', node,
                            [c_node, h_node, tdef_h_node, t_h_node])
    task.set_run_after(self.env.IOPC_TASK)
    self.bld.add_to_group(task, 'code_generation')
    self.source.append(c_node)

# }}}
