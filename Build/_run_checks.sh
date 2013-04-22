#!/bin/bash -e
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2013 INTERSEC SA                                   #
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

    post_process()
    {
        sed -e "s/ pass / \x1B[1;32mpass\x1B[0m /" \
            -e "s/ todo-pass / \x1B[1;33mtodo-pass\x1B[0m /" \
            -e "s/ fail / \x1B[1;31mfail\x1B[0m /" \
            -e "s/ todo-fail / \x1B[1;35mtodo-fail\x1B[0m /" \
            -e "s/ skip / \x1B[1;30mskip\x1B[0m /" \
            -e "s/#.*$/\x1B[1;30m\0\x1B[0m/" \
            -e "s/^:\(.*\)/\x1B[1;31m: \x1B[1;33m\1\x1B[0m/"
    }
else
    say_color()
    {
        shift 1
        echo "$@"
    }
    BEHAVE_FLAGS="--no-color"

    post_process()
    {
        cat
    }
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

tmp=$(mktemp)
tmp2=$(mktemp)
trap "rm $tmp $tmp2" 0

"$(dirname "$0")"/_list_checks.sh "$where" | (
export Z_BEHAVE=1
export Z_HARNESS=1
export Z_TAG_SKIP="${Z_TAG_SKIP:-slow upgrade}"
export Z_MODE="${Z_MODE:-fast}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-handle_segv=0}"
while read t; do
    say_color info "starting suite $t..."
    case ./"$t" in
        */behave)
            res="$pybin -m z  $BEHAVE_FLAGS --format z --no-summary --tags=-web --tags=-slow --tags=-upgrade $(dirname "./$t")/ci/features"
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
        say_color error "TEST SUITE $t FAILED"
    fi
done
) | tee $tmp | post_process

res=1
if $pybin -m z < $tmp > $tmp2; then
    res=0
fi
cat $tmp2 | post_process
exit $res
