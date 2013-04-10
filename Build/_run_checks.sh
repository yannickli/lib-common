#!/bin/bash -e
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2012 INTERSEC SAS                                  #
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
    BEHAVE_FLAGS="--no-color"
fi


for bin in "python2.7" "python2.6"; do
    if $bin -V &> /dev/null ; then
        pybin="$bin"
        break
    fi
done

if [ -z "$pybin" ] ; then
    say_color error "python 2.6 or 2.7 required to run tests"
    exit 1
fi

"$(dirname "$0")"/_list_checks.sh "$where" | (
_err=0
export Z_HARNESS=1
export Z_TAG_SKIP="${Z_TAG_SKIP:-slow upgrade}"
export Z_MODE="${Z_MODE:-fast}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-handle_segv=0}"
while read t; do
    say_color info "starting suite $t..."
    case ./"$t" in
        */behave)
            res="$pybin $(which behave) $BEHAVE_FLAGS --tags=-web --tags=-slow --tags=-upgrade $(dirname "./$t")/ci/features"
            ;;
        *.py)
            res="$pybin ./$t"
            ;;
        *)
            res="./$t"
            ;;
    esac

    if $res ; then
        say_color pass "done"
    else
        _err=1
        say_color error "TEST SUITE $t FAILED"
    fi
done
exit $_err
)
