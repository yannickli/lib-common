##########################################################################
#                                                                        #
#  Copyright (C) 2004-2011 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

include $(var/cfgdir)/cflags.mk

GCCVERSION = $(shell gcc -dumpversion)

CFLAGS += -DNDEBUG -fvisibility=hidden
CXXFLAGS += -DNDEBUG -fvisibility=hidden
LDFLAGS += -Wl,-x

ifeq "$(GCCVERSION)" "4.6.0"
	CFLAGS += -flto -fuse-linker-plugin
	CXXFLAGS += -flto -fuse-linker-plugin
endif
