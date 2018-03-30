from itertools import chain

import waflib.TaskGen as TaskGen


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

@TaskGen.feature('c', 'cprogram', 'cstlib')
@TaskGen.before_method('process_rule')
def prepare_whole(self):
    use_whole = self.to_list(getattr(self, 'use_whole', []))
    if not use_whole:
        return
    self.use = self.to_list(self.use)
    for uw in use_whole:
        if uw in self.use:
            self.bld.fatal(('`{}` from `use_whole` of target `{}` is '
                             'already in attribute `use`, you may remove it')
                            .format(uw, self.target))
        else:
            self.use.append(uw)

@TaskGen.feature('c', 'cprogram', 'cstlib')
@TaskGen.after_method('process_use')
def process_whole(self):
    use_whole = self.to_list(getattr(self, 'use_whole', []))
    if not use_whole:
        return

    self.env.append_value('LINKFLAGS',
                          '-Wl,-Bstatic,--as-needed,--whole-archive')

    for name in use_whole:
        lib = self.bld.get_tgen_by_name(name)
        self.env.STLIB.remove(name)

        # TODO: filter STLIB_PATH by removing unused ones.
        self.env.append_value(
            'LINKFLAGS',
            list(chain.from_iterable(('-Xlinker', p.relpath())
                                     for p in lib.link_task.outputs)))

    self.env.append_value('LINKFLAGS', '-Wl,--no-whole-archive')

# }}}
