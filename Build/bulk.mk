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

# Make complains if all doesn't exist, have it
all::
.PHONY: all

$!deps.mk: $/configure
	mkdir -p $(@D)
	$< -p $(var/profile) -o $@
-include $!deps.mk

ifneq (__dump_targets,$(MAKECMDGOALS))
FORCE: ;
.PHONY: FORCE

# implicit rule to generate a given directory
%/.exists:
	mkdir -p $@
.PRECIOUS: %/.exists
endif

ifeq ($(realpath $(firstword $(MAKEFILE_LIST))),$!Makefile)
include $(var/toolsdir)/bulk-library.mk
include $(var/toolsdir)/bulk-build.mk
else
include $(var/toolsdir)/bulk-library.mk
include $(var/toolsdir)/bulk-intree.mk
endif
