##########################################################################
#                                                                        #
#  Copyright (C) 2004-2006 INTERSEC                                      #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

DEPTH=..
include $(DEPTH)/admin/header.mk

__EXE__   =
__LIB__   = libcommon
__CHECK__ = test

libcommon_SOURCES = \
	array.c \
	blob.c \
	concatbin.c  \
	err_report.c  \
	fifo.c \
	ioveclist.c \
	string_is.c \
	timeval.c \
	msg_template.c \
	unix.c

test_SOURCES = $(libcommon_SOURCES) check.c

include $(DEPTH)/admin/footer.mk
