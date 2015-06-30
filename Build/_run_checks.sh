#!/bin/bash
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2015 INTERSEC SA                                   #
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
libcommondir=$(dirname "$(dirname "$(readlink -f "$0")")")
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

if [ "${OS}" == "darwin" ]; then
    tmp=$(mktemp -t tmp)
    tmp2=$(mktemp -t tmp)
else
    tmp=$(mktemp)
    tmp2=$(mktemp)
fi
trap "rm $tmp $tmp2" 0

set_www_env() {
    case "$Z_TAG_SKIP" in *web*) return 0;; esac

    productdir=$(readlink -e "$1")
    htdocs="$productdir"/www/htdocs/
    [ -d $htdocs ] || return 0;
    # configure the product website spool (cache dir, .htaccess...)
    make -C $htdocs

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


"$(dirname "$0")"/_list_checks.sh "$where" | (
export Z_BEHAVE=1
export Z_HARNESS=1
export Z_TAG_SKIP="${Z_TAG_SKIP:-wip slow upgrade web}"
export Z_MODE="${Z_MODE:-fast}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-handle_segv=0}"

TAGS=($Z_TAG_SKIP)
for TAG in ${TAGS[@]}
do
     BEHAVE_FLAGS="${BEHAVE_FLAGS} --tags=-$TAG"
done
export BEHAVE_FLAGS="$BEHAVE_FLAGS --format z --no-summary"

while read -r zd line; do
    t="${zd}${line}"
    say_color info "starting suite $t..."

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
        *)
            ./$t
            res=$?
            ;;
    esac

    if [ $res -eq 0 ] ; then
        end=$(date '+%s')
        say_color pass "done ($((end - start)) seconds)"
    else
        end=$(date '+%s')
        say_color error "TEST SUITE $t FAILED ($((end - start)) seconds)"
    fi
done
) | tee $tmp | post_process

res=1
if $pybin -m z < $tmp > $tmp2; then
    res=0
fi
cat $tmp2 | post_process
exit $res
