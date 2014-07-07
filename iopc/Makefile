##########################################################################
#                                                                        #
#  Copyright (C) 2004-2014 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

none_LIBRARIES = iopc

_LIBS = -ldl -lpthread -lrt

iopc_SOURCES = \
    iopc-lex.l                \
    iopc-parser.c             \
    iopc-pretty.c             \
    iopc-typer.blk            \
    iopc.tokens               \
    iopc-write.c              \
    \
    iopc-lang-c.blk           \
    iopc-lang-json.blk        \
    \
    $llibcommon.a

include ../Build/base.mk
