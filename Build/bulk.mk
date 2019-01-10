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

include $(var/toolsdir)/bulk-library.mk
ifeq (0,$(MAKELEVEL))
-include $!deps.mk
else
ifneq (,$(filter clean distclean __%,$(MAKECMDGOALS)))
-include $!deps.mk
else
include $!deps.mk
endif
endif

doc:
all check fast-check clean distclean::
FORCE: ;
.PHONY: all check fast-check clean distclean doc FORCE

var/sources    = $(sort $(foreach v,$(filter %_SOURCES,$(.VARIABLES)),$($v)))
var/cleanfiles = $(sort $(foreach v,$(filter %_CLEANFILES,$(.VARIABLES)),$($v)))
var/generated  = $(sort $(foreach f,$(filter ext/gen/%,$(.VARIABLES)),$(call $f,$(var/sources))))

var/staticlibs = $(foreach v,$(filter %_LIBRARIES,$(filter-out %_SHARED_LIBRARIES,$(.VARIABLES))),$($v))
var/sharedlibs = $(foreach v,$(filter %_SHARED_LIBRARIES,$(.VARIABLES)),$($v))
var/programs   = $(foreach v,$(filter %_PROGRAMS,$(.VARIABLES)),$($v))
var/datas      = $(foreach v,$(filter %_DATAS,$(.VARIABLES)),$($v))
var/docs       = $(foreach v,$(filter %_DOCS,$(.VARIABLES)),$($v))

ifeq ($(realpath $(firstword $(MAKEFILE_LIST))),$!Makefile)
##########################################################################
# {{{ Inside the build system
clean::
	find $~ -maxdepth 1 -type f \! -name Makefile \! -name vars.mk -print0 | xargs -0 $(RM)
	$(call fun/expand-if2,$(RM),$(filter-out %/,$(_CLEANFILES)))
	$(call fun/expand-if2,$(RM) -r,$(filter %/,$(_CLEANFILES)))
distclean::
	$(msg/rm) generated files
	$(call fun/expand-if2,$(RM),$(var/generated))
	$(call fun/expand-if2,$(RM),$(filter-out %/,$(var/cleanfiles)))
	$(call fun/expand-if2,$(RM) -r,$(filter %/,$(var/cleanfiles)))
	$(RM) $(filter-out %/,$(DISTCLEANFILES))
	$(RM) -r $(filter %/,$(DISTCLEANFILES))
	$(msg/rm) copied targets
	$(call fun/expand-if2,$(RM),$(var/docs))
	$(call fun/expand-if2,$(RM),$(var/datas))
	$(call fun/expand-if2,$(RM),$(var/programs:=$(EXEEXT)))
	$(call fun/expand-if2,$(RM),$(var/sharedlibs:=.so*))
	$(call fun/expand-if2,$(RM),$(var/staticlibs:=.a) $(var/staticlibs:=.wa))
	$(msg/rm) build system
	$(RM) -r $~
check:: all
	$(var/toolsdir)/_run_checks.sh .
fast-check:: all
	Z_MODE=fast Z_TAG_SKIP='upgrade slow' $(var/toolsdir)/_run_checks.sh .

tags: $(filter-out %.blk.c %.blkk.cc,$(var/generated))
jshint:
.PHONY: tags cscope jshint

define fun/subdirs-targets
$(foreach d,$1,
$(patsubst ./%,%,$(dir $(d:/=)))all::       $(d)all
$(patsubst ./%,%,$(dir $(d:/=)))doc:       $(d)doc
$(patsubst ./%,%,$(dir $(d:/=)))clean::     $(d)clean
$(d)all::
$(d)doc:
$(d)check:: $(d)all
	$(var/toolsdir)/_run_checks.sh $(d)
$(d)fast-check:: $(d)all
	Z_MODE=fast Z_TAG_SKIP='upgrade slow' $(var/toolsdir)/_run_checks.sh $(d)
$(d)clean::
	find $~$(d) -type f \! -name vars.mk -print0 | xargs -0 $(RM)
	$(call fun/expand-if2,$(RM),$(filter-out %/,$($(d)_CLEANFILES)))
	$(call fun/expand-if2,$(RM) -r,$(filter %/,$($(d)_CLEANFILES)))
$(d)distclean:: distclean
)
endef
$(eval $(call fun/subdirs-targets,$(patsubst $/%Makefile,%,$(var/makefiles))))

$(foreach p,$(var/staticlibs),$(eval $(call rule/staticlib,$p)))
$(foreach p,$(var/sharedlibs),$(eval $(call rule/sharedlib,$p)))
$(foreach p,$(var/programs),$(eval $(call rule/program,$p)))
$(foreach p,$(var/datas),$(eval $(call rule/datas,$p)))
$(foreach p,$(var/docs),$(eval $(call rule/docs,$p)))
# }}}
else
##########################################################################
# {{{ Only at toplevel

__setup_buildsys_trampoline:
__setup_buildsys_doc:
.PHONY: __setup_buildsys_trampoline __setup_buildsys_doc

ifeq (0,$(MAKELEVEL))

$!deps.mk: $/configure $(var/toolsdir)/configure.inc.sh
	mkdir -p $(@D)
	$< -p $(var/profile) -o $@ || ($(RM) $@; $(msg/alert) 'configure failed')

__setup_buildsys_trampoline: $!deps.mk
	$(msg/echo) 'checking build-system ...'
	$(msg/echo) 'make: Entering directory `$(var/builddir)'"'"
	$(MAKEPARALLEL) __setup_buildsys
	$(msg/echo) 'make: Entering directory `$(var/srcdir)'"'"

toplevel:
.PHONY: toplevel

all:: toplevel
all check fast-check clean distclean:: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)$@

__setup_buildsys_doc: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)doc

doc: | __setup_buildsys_doc

tags: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile tags
	@$(if $(shell which ctags),,$(error "Please install ctags: apt-get install exuberant-ctags"))
	cd $/ && ctags -o .tags --recurse=yes --totals=yes --links=no \
	    --c-kinds=+p --c++-kinds=+p --fields=+iaS --extra=+q \
	    -I 'qv_t qm_t qh_t IOP_RPC_IMPL IOP_RPC_CB' \
	    --langmap=c:+.blk --langmap=c++:+.blkk \
	    --langdef=iop --langmap=iop:.iop \
	    --regex-iop='/^struct[ ]*([a-zA-Z]+)/\1/s, struct/' \
	    --regex-iop='/^union[ ]*([a-zA-Z]+)/\1/u, union/' \
	    --regex-iop='/^enum[ ]*([a-zA-Z]+)/\1/e, enum/' \
	    --regex-iop='/^interface[ ]*([a-zA-Z]+)/\1/n, interface/' \
	    --exclude=".build*" --exclude="Build" --exclude="Config" \
	    --exclude=".git" --exclude=".svn" --exclude="CVS" \
	    --exclude="old" --exclude="new" --exclude="ogu" --exclude="xxx" \
	    --exclude="*.exe" --exclude="*.js" --exclude="*.blk.c" --exclude="*.blkk.cc" \
	    --exclude="*.swf" --exclude="*.ini" --exclude="*fake.c" --exclude="compat.c"

etags: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile tags
	@$(if $(shell which etags.emacs),,$(error "etags.emacs not found"))
	cd $/ && $(RM) TAGS                                             \
		&&                                                      \
		find . -name \*.hh -or -name \*.blkk                    \
		       -or \( -name \*.cc -and -not -name \*.blkk.cc \) \
			-print0 | xargs -0 etags.emacs -a -l c++ -      \
		&&                                                      \
		find . -name \*.h -or -name \*.blk                      \
		       -or \( -name \*.c -and -not -name \*.blk.c \)    \
			-print0 | xargs -0 etags.emacs -a -l c -

cscope: | __setup_buildsys_trampoline
	cd $/ && \
		find . -regextype sed \
		-regex ".*/[^.]*\.\(c\|cpp\|blk\|blkk\|h\|inc\.c\)$$" \
		-type f -print \
		| grep -v "iop-compat.h" \
		| grep -v "\/compat\/" \
		| sort > .cscope.files && \
		cscope -I$/lib-common/compat -I$/ -ub -i.cscope.files -f.cscope.out

jshint: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile jshint
	@$(if $(shell which jshint),,$(error "Please install jshint: npm install -g jshint"))
	git ls-files -- '*.js' | xargs jshint

ignore:
	$(foreach v,$(CLEANFILES:/=),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/generated),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/datas),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/docs),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/programs:=$(EXEEXT)),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/sharedlibs:=.so),grep -q '^/$v[*]$$' .gitignore || echo '/$v*' >> .gitignore;)
endif
_generated_hdr:
_generated: _generated_hdr
	$(msg/echo) ' ... generating sources done'
.PHONY: _generated_hdr _generated
# }}}
##########################################################################
# {{{ target exports from the build system
ifeq (,$(findstring p,$(MAKEFLAGS)))

$(sort $(var/generated) $(var/datas)) \
$(var/docs)                   \
$(var/programs:=$(EXEEXT))    \
$(var/sharedlibs:=.so)        \
$(var/staticlibs:=.a)         \
$(var/staticlibs:=.pic.a)     \
$(var/staticlibs:=.wa)        \
$(var/staticlibs:=.pic.wa)    \
: FORCE | __setup_buildsys_trampoline
	$(msg/echo) 'building `$@'\'' ...'
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)$@

endif
# }}}
##########################################################################
# {{{ __setup_buildsys
#
#   This target uses costly things so we hide it most of the time
ifeq (__setup_buildsys,$(MAKECMDGOALS))

tmp/makefiles := $(shell find "$(var/srcdir)" -name Makefile -type f \( -path '*/.*' -prune -o -path '*/old/*' -prune -o -print \) | while read file; do \
                         grep -q 'include.*base.mk' $$file && echo $$file; done)
tmp/vars      := $(patsubst $(var/srcdir)/%Makefile,$(var/builddir)/%vars.mk,$(tmp/makefiles))

$(tmp/vars): MAKEFLAGS=rsC
$(tmp/vars): $(var/builddir)%/vars.mk: $(var/srcdir)%/Makefile $!deps.mk $(var/toolsdir)/*
	$(msg/generate) $(@R)
	mkdir -p $(@D)
	make --no-print-directory -rsC $(var/srcdir)$* __dump_targets > $@

$(var/builddir)/Makefile: $(var/srcdir)/configure $(tmp/vars) $(var/toolsdir)/*
	$(msg/generate) $(@R)
	mkdir -p $(@D)
	echo 'var/makefiles := $(call fun/msq,$(tmp/makefiles))'      >  $@
	(:$(patsubst $/%Makefile,;echo 'include $~%vars.mk',$(tmp/makefiles)))  >> $@
	echo 'include $(var/toolsdir)/base.mk'                        >> $@

__setup_buildsys: $(var/builddir)/Makefile
.PHONY: __setup_buildsys

endif
# }}}
# {{{ __setup_forward

__setup_forward:
.PHONY: __setup_forward

# }}}
##########################################################################
# {{{ __dump_targets
ifeq (__dump_targets,$(MAKECMDGOALS))

__dump_targets: . = $(patsubst $(var/srcdir)/%,%,$(realpath $(CURDIR))/)
__dump_targets:
	echo 'ifneq (,$$(realpath $.Makefile))'
	$(foreach v,$(filter %_DOCS %_DATAS %_PROGRAMS %_LIBRARIES,$(.VARIABLES)),\
	    echo '$v += $(call fun/msq,$(call fun/rebase,$(CURDIR),$($v)))';)
	$(foreach v,$(filter %_DEPENDS %_SOURCES,$(.VARIABLES)),\
	    echo '$.$v += $(call fun/msq,$(call fun/rebase,$(CURDIR),$($v)))';)
	$(foreach v,$(filter %LINKER %LIBS %COMPILER %FLAGS %_SOVERSION %_NOCHECK,$(filter-out MAKE%,$(.VARIABLES))),\
	    echo '$.$v += $(call fun/msq,$($v))';)
	echo '$._CLEANFILES += $(call fun/msq,$(call fun/rebase,$(CURDIR),$(CLEANFILES)))'
	echo 'DISTCLEANFILES += $(call fun/msq,$(call fun/rebase,$(CURDIR),$(DISTCLEANFILES)))'
	echo ''
	make -nspqr | $(var/toolsdir)/_local_targets.sh \
	    "$(var/srcdir)" "$." "$(var/toolsdir)" "$(var/cfgdir)" | \
	    sed -n -e 's~$$~ FORCE ; $$(MAKE) -w$(if $.,C $.) $$(subst $.,,$$@)~p'
	echo 'endif'

.PHONY: __dump_targets

endif
#}}}
endif
