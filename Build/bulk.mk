##########################################################################
#                                                                        #
#  Copyright (C) 2004-2008 INTERSEC SAS                                  #
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

all clean distclean::
FORCE: ;
.PHONY: all clean distclean FORCE

$!deps.mk: $/configure
	mkdir -p $(@D)
	$< -p $(var/profile) -o $@
-include $!deps.mk

ifeq ($(realpath $(firstword $(MAKEFILE_LIST))),$!Makefile)
##########################################################################
# {{{ Inside the build system
clean::
	find $~ -type f \! -name Makefile \! -name vars.mk -print0 | xargs -0 $(RM)
distclean:: | clean
	$(msg/rm) build system
	$(RM) -r $~

define fun/subdirs-targets
$(foreach d,$1,
$(patsubst ./%,%,$(dir $(d:/=)))all::       $(d)all
$(patsubst ./%,%,$(dir $(d:/=)))clean::     $(d)clean
$(d)all::
$(d)clean::
	find $~$(d) -type f \! -name vars.mk -print0 | xargs -0 $(RM)
$(d)distclean:: distclean
)
endef
$(eval $(call fun/subdirs-targets,$(patsubst $/%,%,$(var/subdirs))))

$(foreach p,$(foreach v,$(filter %_LIBRARIES,$(filter-out %_SHARED_LIBRARIES,$(.VARIABLES))),$($v)),$(eval $(call rule/staticlib,$p)))
$(foreach p,$(foreach v,$(filter %_SHARED_LIBRARIES,$(.VARIABLES)),$($v)),$(eval $(call rule/sharedlib,$p)))
$(foreach p,$(foreach v,$(filter %_PROGRAMS,$(.VARIABLES)),$($v)),$(eval $(call rule/program,$p)))
# }}}
else
##########################################################################
# {{{ Only at toplevel
ifeq (0,$(MAKELEVEL))

__setup_buildsys_trampoline:
	$(msg/echo) 'checking build-system ...'
	$(msg/echo) 'make: Entering directory `$(var/builddir)'"'"
	$(MAKEPARALLEL) __setup_buildsys
	$(msg/echo) 'make: Entering directory `$(var/srcdir)'"'"
.PHONY: __setup_buildsys_trampoline

all clean distclean:: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)$@

tags:
	@$(if $(shell which ctags),,$(error "Please install ctags: apt-get install exuberant-ctags"))
	cd $/ && ctags -o .tags --recurse=yes --totals=yes \
                     --exclude=".svn" --exclude=".objs*" --exclude="old" \
                     --exclude="new" --exclude="ogu" --exclude="xxx" \
                     --exclude="*.mk" --exclude="*.exe"

endif
# }}}
##########################################################################
# {{{ target exports from the build system
ifeq (,$(findstring p,$(MAKEFLAGS)))

$(foreach p,$(foreach v,$(filter %_LIBRARIES,$(filter-out %_SHARED_LIBRARIES,$(.VARIABLES))),$($v)),$(eval $(call goal/staticlib,$p)))
$(foreach p,$(foreach v,$(filter %_SHARED_LIBRARIES,$(.VARIABLES)),$($v)),$(eval $(call goal/sharedlib,$p)))
$(foreach p,$(foreach v,$(filter %_PROGRAMS,$(.VARIABLES)),$($v)),$(eval $(call goal/program,$p)))

endif
# }}}
##########################################################################
# {{{ __setup_buildsys
#
#   This target uses costly things so we hide it most of the time
ifeq (__setup_buildsys,$(MAKECMDGOALS))

tmp/subdirs := $(shell '$(var/toolsdir)/_list_subdirs.sh' '$(var/srcdir)')
tmp/vars    := $(patsubst $(var/srcdir)/%,$(var/builddir)/%vars.mk,$(tmp/subdirs))

$(tmp/vars): $(var/builddir)%/vars.mk: $(var/srcdir)%/Makefile $(var/toolsdir)/*
	$(msg/generate) $(@R)
	mkdir -p $(@D)
	$(MAKE) --no-print-directory -rsC $(var/srcdir)$* __dump_targets > $@

$(var/builddir)/Makefile: $(var/srcdir)/configure $(var/toolsdir)/* | $(tmp/vars)
	$(msg/generate) $(@R)
	mkdir -p $(@D)
	echo 'var/subdirs := $(call fun/msq,$(tmp/subdirs))'          >  $@
	(:$(patsubst $/%,;echo 'include $~%vars.mk',$(tmp/subdirs)))  >> $@
	echo 'include $(var/toolsdir)/base.mk'                        >> $@

__setup_buildsys: $(var/builddir)/Makefile
.PHONY: __setup_buildsys

endif
# }}}
##########################################################################
# {{{ __dump_targets
ifeq (__dump_targets,$(MAKECMDGOALS))

__dump_targets: . = $(patsubst $(var/srcdir)/%,%,$(realpath $(CURDIR))/)
__dump_targets:
	$(foreach v,$(filter %_TESTS %_PROGRAMS %_LIBRARIES,$(.VARIABLES)),\
	    echo '$v += $(call fun/msq,$(call fun/rebase,$(CURDIR),$($v)))';)
	$(foreach v,$(filter %_DEPENDS %_SOURCES,$(.VARIABLES)),\
	    echo '$.$v += $(call fun/msq,$(call fun/rebase,$(CURDIR),$($v)))';)
	$(foreach v,$(filter %FLAGS %_SOVERSION,$(filter-out MAKE%,$(.VARIABLES))),\
	    echo '$.$v += $(call fun/msq,$($v))';)
	echo ''
	$(MAKE) -nspqr | $(var/toolsdir)/_local_targets.sh \
	    "$(var/srcdir)" "$." "$(var/toolsdir)" | \
	    sed -n -e 's~$$~ ; $$(MAKE) -w$(if $.,C $.) $$(subst $.,,$$@)~p'

.PHONY: __dump_targets

endif
#}}}
endif
