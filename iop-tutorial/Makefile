##########################################################################
#                                                                        #
#  Copyright (C) 2004-2017 INTERSEC SA                                   #
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
examples_LIBRARIES = iop
ifneq (,$(SWIFTC))
examples_PROGRAMS += ex-iop-swift
endif

_LIBS = -lm -lz -lrt -ldl -lpthread

iop_SOURCES = \
	exiop.iop \
	exiop.iop.swift
iop_SWIFTMODULE = IopTutorial
iop_SWIFTDEPS = libcommon
iop_SWIFTMIXED = 1

ex-iop_SOURCES = \
	ex-iop.c \
	\
	iop.a \
	$llibcommon.a

ex-iop-swift_SOURCES = \
	ex-iop.swift \
	\
	iop.a \
	$llibcommon.a
ex-iop-swift_SWIFTMAIN = 1
ex-iop-swift_SWIFTMODULE = ex_iop

endif

include ../Build/base.mk
