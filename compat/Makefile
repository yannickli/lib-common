##########################################################################
#                                                                        #
#  Copyright (C) 2004-2007 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

DEPTH=..
include $(DEPTH)/admin/header.mk

__LIB__ := compat

compat_SOURCES := compat.c check.c

ifneq (,$(MINGCC))
  # Beeing able to build a "check" program would be a good idea under 
  # MingGW. Needs more work though.
  compat_SOURCES := $(filter-out check.c, $(compat_SOURCES))
endif

include $(DEPTH)/admin/footer.mk
