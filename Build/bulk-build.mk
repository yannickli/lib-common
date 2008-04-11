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

all distclean fastclean::
distclean:: | fastclean
	$(msg/rm) build system
	$(RM) -r $~
clean:: | fastclean
	find $~$(d) -type f \! -name Makefile \! -name vars.mk -print0 | xargs -0 $(RM)
__generate_files:
.PHONY: __generate_files all fastclean clean distclean

define fun/subdirs-targets
$(foreach d,$1,
$(patsubst ./%,%,$(dir $(d:/=)))all::       $(d)all
$(patsubst ./%,%,$(dir $(d:/=)))clean::     $(d)clean
$(patsubst ./%,%,$(dir $(d:/=)))fastclean:: $(d)fastclean
$(d)all $(d)fastclean::
$(d)clean:: | $(d)fastclean
	find $~$(d) -type f \! -name vars.mk -print0 | xargs -0 $(RM)
)
endef
$(eval $(call fun/subdirs-targets,$(patsubst $/%,%,$(var/subdirs))))

$(foreach p,$(foreach v,$(filter %_LIBRARIES,$(filter-out %_SHARED_LIBRARIES,$(.VARIABLES))),$($v)),$(eval $(call rule/staticlib,$p)))
$(foreach p,$(foreach v,$(filter %_SHARED_LIBRARIES,$(.VARIABLES)),$($v)),$(eval $(call rule/sharedlib,$p)))
$(foreach p,$(foreach v,$(filter %_PROGRAMS,$(.VARIABLES)),$($v)),$(eval $(call rule/program,$p)))
