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

_PROGRAMS = str-ctype-maker farchc
_LIBS = -lrt -ldl -lpthread

str-ctype-maker_SOURCES = \
	str-ctype-maker.c \
	\
	$llibcommon.a

farchc_SOURCES = \
	farchc.c \
	\
	$llibcommon.a
farchc_NOGENERATED = 1

include ../Build/base.mk
