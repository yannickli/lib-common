##########################################################################
#                                                                        #
#  Copyright (C) 2004-2010 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

examples_PROGRAMS = ex-iop

_LIBS = -lrt -lm

ex-iop_SOURCES = \
	ex-iop.c \
	exiop.iop \
	\
	$/lib-common/libcommon.a \
	$/lib-common/time-lp-simple.a
ex-iop_LIBS = -lpthread -ldl -lrt -lm

include ../../Build/base.mk
