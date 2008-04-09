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

srcdir="$1"
subdir="$2"
toolsdir="$3"

die() {
    echo 1>&2 "$@"
    exit 1
}

grep -E '^ *[^[:space:]$()#=/][^[:space:]$()#=]*:( |$)' | while read target rest
do
    case "$target" in
        __*:|FORCE:|.*:|Makefile:|%:|"$toolsdir"*:)
            : "ignore some internal stuff";;
        /*:)
            : "ignore absolute stuff, likely to be includes";;
        *:)
            echo "$subdir${target}";;
        *)
	    echo 1>&2 ">>> $target";
            die "You cannot use embeded spaces in target names";;
    esac
done | sort -u
