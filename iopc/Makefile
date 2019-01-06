##########################################################################
#                                                                        #
#  Copyright (C) 2004-2019 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

iopc_LIBRARIES = libiopc
iopc_LIBRARIES += libiopsq
iopc_PROGRAMS = iopc
test_PROGRAMS = zchk

_LIBS = -ldl -lpthread -lrt
_IOPTSPATH = $l/iopc

iopc_SOURCES = \
    iopc.c                          \
    libiopc.a                       \
    $lcore-version.c                \
    $llibcommon.a
iopc_NOGENERATED = 1

zchk_SOURCES = \
    zchk.c \
    libiopsq.a \
    libiopc.a \
    $liop/tstiop.a \
    $llibcommon.a

libiopc_SOURCES = \
    iopc.fc                   \
    iopc-dso.c                \
    iopc-lex.l                \
    iopc-parser.c             \
    iopc-pretty.c             \
    iopc-typer.blk            \
    iopctokens.c              \
    iopc-write.c              \
    \
    iopc-lang-c.blk           \
    iopc-lang-json.blk        \
    iopc-lang-typescript.c
libiopc_NOGENERATED = 1

libiopsq_SOURCES = \
    iopc-iopsq.c              \
    iopsq.iop
libiopsq_NOGENERATED = 1

include ../Build/base.mk
