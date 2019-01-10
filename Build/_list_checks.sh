#!/bin/sh -e
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

CURDIR="$(readlink -e "$(pwd)")"

_err=0
err()
{
    _err=1
    echo "$@" >&2
}

dump_zf()
{
    zf="$1"
    zd="$(dirname "$zf")"

    (
        ifs="$IFS"
        lno=0

        while read line; do
            lno=$(($lno + 1))
            has_match=

            # Ignore comments
            case "$line" in "#"*) continue;; esac

            IFS=''
            # $line is unquoted because IFS='', hence it's safe, yuck
            for f in "$zd/"$line; do
                case ./"$f" in
                    */behave)
                        has_match=b
                        echo "${f#$CURDIR/}"
                        continue
                        ;;
                    *.py)
                        test -f "$f" || continue
                        ;;
                    *)
                        test -x "$f" || continue
                        ;;
                esac

                has_match=t
                f="$(readlink -e "$f")"
                echo "${f#$CURDIR/}"
            done
            test -n "$has_match" || err "$zf:$lno: no match for $line"
            IFS="$ifs"
        done
        exit $_err
    ) < "$zf"
}

find "$1" -type f -name ZFile | (
    while read zf; do
        dump_zf "$zf" || _err=1
    done
    exit $_err
)

exit $_err
