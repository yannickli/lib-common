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

FORTIFY_SOURCE=
include $(var/cfgdir)/profile-default.mk
CFLAGS := $(filter-out -O%,$(CFLAGS))
CFLAGS += -O0 -Wno-uninitialized -fno-inline -fno-inline-functions -g3
CXXFLAGS += -O0 -Wno-uninitialized -fno-inline -fno-inline-functions -g3

ifeq (,$(USE_DWARF4))
CFLAGS += -gdwarf-2
CXXFLAGS += -gdwarf-2
endif
