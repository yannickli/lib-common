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
__CHECK__ = test iconv_test msg_test

libcommon_SOURCES = \
	archive.c \
	array.c \
	str_array.c \
	blob.c \
	blob_zlib.c \
	blob_iconv.c \
	concatbin.c  \
	err_report.c  \
	fifo.c \
	ioveclist.c \
	mmappedfile.c \
	string_is.c \
	strconv.c \
	timeval.c \
	msg_template.c \
	stopper.c \
	showflags.c \
	unix.c

test_SOURCES = $(libcommon_SOURCES) check.c
test_LDFLAGS = -lz

iconv_test_SOURCES = \
	test_blob_iconv.c \
	libcommon.a

msg_test_CFLAGS = -DMSG_TEST
msg_test_SOURCES = msg_template.c libcommon.a

include $(DEPTH)/admin/footer.mk
