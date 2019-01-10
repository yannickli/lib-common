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

examples_PROGRAMS = ex-iop
examples_LIBRARIES = iop

_LIBS = -lm -lz -lrt -ldl -lpthread $(openssl_LIBS)

iop_SOURCES = \
	exiop.iop \

ex-iop_SOURCES = \
	ex-iop.c \
	\
	iop.a \
	$llibcommon.a

include ../Build/base.mk
