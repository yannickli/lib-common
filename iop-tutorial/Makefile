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

ifeq (,$(TOOLS_REPOSITORY))
examples_PROGRAMS = ex-iop
endif

_LIBS = -lm -lz -lrt -ldl -lpthread

ex-iop_SOURCES = \
	ex-iop.c \
	exiop.iop \
	\
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a

include ../Build/base.mk
