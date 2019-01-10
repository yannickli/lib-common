#!/bin/bash
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

set -o pipefail

where="$1"
if which greadlink &> /dev/null; then
    readlink=greadlink
else
    readlink=readlink
fi
libcommondir=$(dirname "$(dirname "$($readlink -f "$0")")")
shift
BEHAVE_FLAGS="${BEHAVE_FLAGS:-}"

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
        sed -e $'s/ pass / \x1B[1;32mpass\x1B[0m /' \
            -e $'s/ todo-pass / \x1B[1;33mtodo-pass\x1B[0m /' \
            -e $'s/ fail / \x1B[1;31mfail\x1B[0m /' \
            -e $'s/ todo-fail / \x1B[1;35mtodo-fail\x1B[0m /' \
            -e $'s/ skip / \x1B[1;30mskip\x1B[0m /' \
            -e $'s/#\(.*\)$/\x1B[1;30m#\\1\x1B[0m/' \
            -e $'s/^:\(.*\)/\x1B[1;31m: \x1B[1;33m\\1\x1B[0m/'
    }
else
    say_color()
    {
        shift 1
        echo "$@"
    }
    BEHAVE_FLAGS="${BEHAVE_FLAGS} --no-color"

    post_process()
    {
        cat
    }
fi

PYTHON_BINARIES=${PYTHON_BINARIES:-"python2.7 python2.6"}
for bin in $PYTHON_BINARIES; do
    if $bin -V &> /dev/null ; then
        pybin="$bin"
        break
    fi
done

if [ -z "$pybin" ] ; then
    say_color error "python 2.6 or greater is required to run tests"
    exit 1
fi

if [ "${OS}" == "darwin" ]; then
    tmp=$(mktemp -t tmp)
    tmp2=$(mktemp -t tmp)
    corelist=$(mktemp -t tmp)
else
    tmp=$(mktemp)
    tmp2=$(mktemp)
    corelist=$(mktemp)
fi
trap "rm $tmp $tmp2 $corelist" 0

set_www_env() {
    if [[ "$Z_TAG_SKIP" =~ "web" ]] && [[ -z "$Z_TAG_OR" ]]; then
        return 0
    fi

    productdir=$($readlink -e "$1")
    htdocs="$productdir"/www/htdocs/
    htdocs_target="all"

    if [[ $P = "coverage" ]]; then
        make -q -C $htdocs $P
        if [[ $? != 2 ]]; then
            htdocs_target="$P"
        else
            say_color error "The Makefile in $htdocs does not contain the"\
                            " coverage target, therefore the web coverage"\
                            " can't be computed for that product"
        fi
    fi

    [ -d $htdocs ] || return 0;
    # configure the product website spool (cache dir, .htaccess...)
    make -C $htdocs $htdocs_target

    z_www="${Z_WWW:-$(dirname "$libcommondir")/www/www-spool}"
    index=$(basename "$productdir").php
    intersec_so=$(find $(dirname "$productdir") -name intersec.so -print -quit)
    Z_WWW_HOST="${Z_WWW_HOST:-$(hostname -f)}"
    Z_WWW_PREFIX="${Z_WWW_PREFIX:-zselenium}"
    Z_WWW_BROWSER="${Z_WWW_BROWSER:-Remote}"
    # configure an apache website and add intersec.so to the php configuration
    make -C "$z_www" all htdocs=$htdocs index=$index intersec_so=$intersec_so \
                         host="${Z_WWW_PREFIX}.${Z_WWW_HOST}"
    if [ $? -ne 0 ]; then
        echo -e "****** Error ******\n"                                       \
            "To run web test suite you need to have some privileges:\n"       \
            " write access on: /etc/apache2/sites* | /etc/httpd/conf.d\n"     \
            " write access on: /etc/php5/conf.d | /etc/php.d/ \n"             \
            " sudoers without pwd on: /etc/init.d/apache2 | /etc/init.d/httpd"
        return 1
    fi
    export Z_WWW_HOST Z_WWW_PREFIX Z_WWW_BROWSER
}


"$(dirname "$0")"/_list_checks.py "$where" | (
export Z_BEHAVE=1
export Z_HARNESS=1
export Z_TAG_SKIP="${Z_TAG_SKIP:-wip slow upgrade web perf}"
export Z_TAG_OR="${Z_TAG_OR:-}"
export Z_MODE="${Z_MODE:-fast}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-handle_segv=0}"

for TAG_OR in ${Z_TAG_OR[@]}
do
    COMA_SEPARATED_TAGS=",$TAG_OR$COMA_SEPARATED_TAGS"
done

TAGS=($Z_TAG_SKIP)
for TAG in ${TAGS[@]}
do
     if [[ $TAG = "wip" ]]; then
         BEHAVE_TAGS="${BEHAVE_TAGS} --tags=-$TAG"
     else
         BEHAVE_TAGS="${BEHAVE_TAGS} --tags=-$TAG$COMA_SEPARATED_TAGS"
    fi
done
export BEHAVE_FLAGS="${BEHAVE_FLAGS} ${BEHAVE_TAGS} --format z --no-summary"

coredump=$(which core_dump)

while read -r zd line; do
    t="${zd}${line}"
    say_color info "starting suite $t..."
    [ -n "$coredump" ] && $pybin $coredump list > $corelist

    start=$(date '+%s')
    case ./"$t" in
        */behave)
            productdir=$(dirname "./$t")
            res=1
            set_www_env $PWD/"$productdir"
            if [ $? -eq 0 ]; then
                $pybin -m z $BEHAVE_FLAGS "$productdir"/ci/features
                res=$?
            fi
            ;;
        *.py)
            $pybin ./$t
            res=$?
            ;;
        *testem.json)
            cd $zd
            ztestem $line
            res=$?
            cd - &>/dev/null
            ;;
        */check_php)
            $pybin $(which check_php) "$zd"
            res=$?
            ;;
        *)
            ./$t
            res=$?
            ;;
    esac
    [ -n "$coredump" ] && $pybin $coredump --format z -i @$corelist -r $PWD diff

    if [ $res -eq 0 ] ; then
        end=$(date '+%s')
        say_color pass "done ($((end - start)) seconds)"
    else
        end=$(date '+%s')
        say_color error "TEST SUITE $t FAILED ($((end - start)) seconds)"
    fi
done
) | tee $tmp | post_process

# Thanks to pipefail, it is not zero if at least one process failed
res=$?
if [ $res -ne 0 ]; then
    say_color error "check processes failed. check head or tail of log output"
fi

# whatever the previous status, set an error if a test failed
if ! $pybin -m z < $tmp > $tmp2; then
    res=1
fi
cat $tmp2 | post_process
exit $res
