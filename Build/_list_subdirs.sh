#!/bin/sh -e
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2008 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

dump() {
    while test $# != 0; do
        if test -f "$1/Makefile" && grep -q 'include.*base.mk' "$1/Makefile"; then
            echo "$1/"
            dump $(find "$1" -mindepth 1 -maxdepth 1 \! -name '.*' -type d)
        fi
        shift
    done
}

dump "${1%/}"
