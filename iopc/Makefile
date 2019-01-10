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

iopc_LIBRARIES = libiopc
iopc_PROGRAMS = iopc

_LIBS = -ldl -lpthread -lrt

iopc_SOURCES = \
    iopc.c                          \
    libiopc.a                       \
    $lcore-version.c                \
    $llibcommon.a

libiopc_SOURCES = \
    iopc.fc                   \
    iopc-dso.c                \
    iopc-lex.l                \
    iopc-parser.c             \
    iopc-pretty.c             \
    iopc-typer.blk            \
    iopc.tokens               \
    iopc-write.c              \
    \
    iopc-lang-c.blk           \
    iopc-lang-json.blk        \
    iopc-lang-swift.c

include ../Build/base.mk
