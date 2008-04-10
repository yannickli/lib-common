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

##########################################################################
# Only at toplevel
#
ifeq (0,$(MAKELEVEL))
__setup_buildsys_trampoline:
	$(msg/echo) 'checking build-system ...'
	$(msg/echo) 'make: Entering directory `$(var/builddir)'"'"
	$(MAKEPARALLEL) __setup_buildsys
	$(msg/echo) 'make: Entering directory `$(var/srcdir)'"'"

tags:
	@$(if $(shell which ctags),,$(error "Please install ctags: apt-get install exuberant-ctags"))
	cd $/ && ctags -o .tags --recurse=yes --totals=yes \
                     --exclude=".svn" --exclude=".objs*" --exclude="old" \
                     --exclude="new" --exclude="ogu" --exclude="xxx" \
                     --exclude="*.mk" --exclude="*.exe"
else
__setup_buildsys_trampoline: ;
endif

%: | __setup_buildsys_trampoline ;
.PHONY: %

distclean:: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile distclean

all fastclean clean:: FORCE | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)$@

.PHONY: all fastclean clean distclean __setup_buildsys_trampoline

ifeq (,$(findstring p,$(MAKEFLAGS)))
define fun/forward
$1: | __setup_buildsys_trampoline
	$(msg/echo) 'building `$1'\'' ...'
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)$1
.PHONY: $1
endef
$(foreach p,$(foreach v,$(filter %_PROGRAMS,$(.VARIABLES)),$($v)),$(eval $(call fun/forward,$p$(EXEEXT))))
$(foreach p,$(foreach v,$(filter %_SHARED_LIBRARIES,$(.VARIABLES)),$($v)),$(eval $(call fun/forward,$p.so)))
$(foreach p,$(foreach v,$(filter %_LIBRARIES,$(filter-out %_SHARED_LIBRARIES,$(.VARIABLES))),$($v)),$(eval $(call fun/forward,$p.a)))
$(foreach p,$(foreach v,$(filter %_LIBRARIES,$(filter-out %_SHARED_LIBRARIES,$(.VARIABLES))),$($v)),$(eval $(call fun/forward,$p.wa)))
endif

##########################################################################
# __setup_buildsys
#
#   This target uses costly things so we hide it most of the time
#
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
	echo 'var/subdirs := $(call fun/msq,$(tmp/subdirs))'         >  $@
	(:$(foreach s,$(patsubst $/%,$!%,$(tmp/subdirs)),;echo 'include $svars.mk'))  >> $@
	echo 'include $(var/toolsdir)/base.mk'                       >> $@

__setup_buildsys: $(var/builddir)/Makefile $(var/builddir)/depends.mk
.PHONY: __setup_buildsys

endif

##########################################################################
# __dump_targets
#
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
	$(MAKE) -rpqs | $(var/toolsdir)/_local_targets.sh \
	    "$(var/srcdir)" "$." "$(var/toolsdir)" | \
	    sed -n -e '/::/d; s~$$~ ; $$(MAKE) -wC $. $$(subst $.,,$$@)~p'

.PHONY: __dump_targets

endif
