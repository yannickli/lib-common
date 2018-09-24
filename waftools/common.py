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

'''
Contains the code that could be useful for both backend and frontend build.
'''

import os

# pylint: disable = import-error
from waflib import Build
from waflib import TaskGen

from waflib.Build import BuildContext
from waflib.Context import Context
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
    env = dict(os.environ)

    if ctx.cmd == 'fast-check':
        env['Z_MODE']     = 'fast'
        env['Z_TAG_SKIP'] = 'upgrade slow perf'
    elif ctx.cmd == 'www-check':
        env['Z_LIST_SKIP'] = 'C behave'
    elif ctx.cmd == 'selenium':
        env['Z_LIST_SKIP']  = 'C web'
        env['Z_TAG_SKIP']   = 'wip'
        env['BEHAVE_FLAGS'] = '--tags=web'
    elif ctx.cmd == 'fast-selenium':
        env['Z_LIST_SKIP']  = 'C web'
        env['Z_TAG_SKIP']   = 'wip upgrade slow'
        env['BEHAVE_FLAGS'] = '--tags=web'
    elif ctx.cmd != 'check':
        return

    path = ctx.launch_node().path_from(ctx.env.PROJECT_ROOT)
    cmd = '{0} {1}'.format(ctx.env.RUN_CHECKS_SH[0], path)
    if ctx.exec_command(cmd, stdout=None, stderr=None, env=env):
        ctx.fatal('')

class CheckClass(BuildContext):
    '''run tests (no web)'''
    cmd = 'check'

class FastCheckClass(BuildContext):
    '''run tests in fast mode (no web)'''
    cmd = 'fast-check'

class WwwCheckClass(BuildContext):
    '''run jasmine tests'''
    cmd = 'www-check'

class SeleniumCheckClass(BuildContext):
    '''run selenium tests (including slow ones)'''
    cmd = 'selenium'

class FastSeleniumCheckClass(BuildContext):
    '''run selenium tests (without slow ones)'''
    cmd = 'fast-selenium'


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
# {{{ get_env_bool


def get_env_bool(self, name):
    val = os.environ.get(name, 0)
    if isinstance(val, basestring):
        return val.lower() in ['true', 'yes', '1']
    else:
        return int(val) == 1


Context.get_env_bool = get_env_bool


# }}}

# {{{ build

def build(ctx):
    # Set post_mode to POST_AT_ONCE, which allows to properly handle
    # dependencies between web task generators and IOP ones
    # cf. https://gitlab.com/ita1024/waf/issues/2191
    ctx.post_mode = Build.POST_AT_ONCE

    ctx.env.PROJECT_ROOT = ctx.srcnode
    ctx.UseGroup = UseGroup

    # Register pre/post functions
    ctx.add_post_fun(run_checks)


# }}}
