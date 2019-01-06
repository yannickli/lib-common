#!/bin/sh
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

set -e

flex -R -o $2 $1

sed -i -e 's/^extern int isatty.*;//' \
       -e '1s/^/#if defined __clang__ || ((__GNUC__ << 16) + __GNUC_MINOR__ >= (4 << 16) +2)\n#pragma GCC diagnostic ignored "-Wsign-compare"\n#endif\n/' \
       -e 's/^\t\tint n; \\/            size_t n; \\/' \
       -e 's/^int .*get_column.*;//' \
       -e 's/^void .*set_column.*;//' \
       -e 's!$~$3.c+"!$(3:l=c)"!g' \
       $2
