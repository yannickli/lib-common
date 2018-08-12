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

# pylint: disable = import-error
from waflib import TaskGen

from waflib.Build import BuildContext
from waflib.Node import Node
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
# {{{ with UseGroup statement


"""
This context manager allows using a waf group, and then restore the previous
one.

For example, this:

   with UseGroup(ctx, 'www'):
       do_something()

Is equivalent to:

   previous_group = ctx.current_group
   ctx.set_group('www')
   do_something()
   ctx.set_group(previous_group)
"""
class UseGroup(object):

    def __init__(self, ctx, group):
        self.ctx = ctx
        self.group = group

    def __enter__(self):
        self.previous_group = self.ctx.current_group
        self.ctx.set_group(self.group)

    def __exit__(self, exc_type, exc_value, traceback):
        self.ctx.set_group(self.previous_group)


# }}}

# {{{ build

def build(ctx):
    ctx.env.PROJECT_ROOT = ctx.srcnode

    ctx.UseGroup = UseGroup

    # Register pre/post functions
    ctx.add_post_fun(run_checks)


# }}}
