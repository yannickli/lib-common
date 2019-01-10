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

NOASSERT ?= 1
include $(var/cfgdir)/cflags.mk
CFLAGS += -fvisibility=hidden
CXXFLAGS += -fvisibility=hidden
LDFLAGS += -Wl,-x -rdynamic

NODE_ENV=production
WEBPACK_MODE=production
