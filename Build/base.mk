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

include $(dir $(realpath $(lastword $(MAKEFILE_LIST))))/base-vars.mk

#
# clean, distclean, and __* magic rules must be called alone
#
ifneq (,$(filter clean distclean __%,$(MAKECMDGOALS)))
    ifneq (1,$(words $(MAKECMDGOALS)))
        target = $(firstword $(filter clean distclean __%,$(MAKECMDGOALS)))
        $(error target '$(target)' must be used alone)
    endif
endif

include $(var/toolsdir)/base-utils.mk
include $(var/toolsdir)/bulk.mk
