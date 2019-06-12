#!/bin/bash -u
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

RES=0

run_test() {
    echo
    echo "# $1"
    eval "$1"
    let "RES=$RES + $?"
}

main() {
    local tools_python_dir
    local waf_list

    tools_python_dir=$(dirname "$(readlink -f "$0")")
    cd "$tools_python_dir" || exit -1
    waf_list="$(waf list)"

    if grep -qs "iopy/python2/iopy" <<<"$waf_list" ; then
        run_test "python2 $tools_python_dir/z_iopy.py"
    fi

    if grep -qs "zchk-iopy-dso2" <<<"$waf_list" ; then
        run_test "$tools_python_dir/zchk-iopy-dso2"
    fi

    if grep -qs "iopy/python3/iopy" <<<"$waf_list" ; then
        run_test "python3 $tools_python_dir/z_iopy.py"
    fi

    if grep -qs "zchk-iopy-dso3" <<<"$waf_list" ; then
        run_test "$tools_python_dir/zchk-iopy-dso3"
    fi
}

main
exit $RES
