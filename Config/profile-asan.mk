##########################################################################
#                                                                        #
#  Copyright (C) 2004-2013 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

CC=clang
CXX=clang
include $(var/cfgdir)/profile-debug.mk
CNOPICFLAGS += -g3 -fsanitize=address -fno-omit-frame-pointer
CXXNOPICFLAGS += -g3 -fsanitize=address -fno-omit-frame-pointer
LDFLAGS += -g3 -fsanitize=address
