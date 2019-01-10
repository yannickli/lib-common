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

CC := i586-mingw32msvc-gcc
AR := i586-mingw32msvc-ar

include $(var/cfgdir)/profile-default.mk

CFLAGS += -DMINGCC -I/usr/i586-mingw32msvc/include \
	  -U__SIZE_TYPE__ -D__SIZE_TYPE__=unsigned \
	  -D_SSIZE_T_ -Dssize_t=int
LIBS += -lws2_32

# don't warn about comparisons signed/unsigned comparisons: (FD_SET problem)
CFLAGS += -Wno-sign-compare
