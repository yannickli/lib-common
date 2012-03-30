#!/bin/sh -e
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2011 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

where="$1"
shift

if test "$TERM" != "dumb" -a -t 1 &&
    tput bold >/dev/null 2>&1 &&
    tput setaf 1 >/dev/null 2>&1 &&
    tput sgr0 >/dev/null 2>&1
then
    say_color()
    {
        case "$1" in
            error) tput bold; tput setaf 1;;
            pass)  tput bold; tput setaf 2;;
            info)  tput setaf 3;;
        esac
        shift
        printf "%s" "$*"
        tput sgr0
        echo
    }
else
    say_color()
    {
        shift 1
        echo "$@"
    }
fi


"$(dirname "$0")"/_list_checks.sh "$where" | (
_err=0
while read t; do
    say_color info "starting suite $t..."
    if Z_HARNESS=1 Z_TAG_SKIP="${Z_TAG_SKIP:-slow upgrade}" Z_MODE="${Z_MODE:-fast}" ./"$t"; then
        say_color pass "done"
    else
        _err=1
        say_color error "TEST STUITE $t FAILED"
    fi
done
exit $_err
)
