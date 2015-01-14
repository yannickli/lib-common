#!/bin/bash -e
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

if [ "${OS}" == "darwin" ]; then
    CURDIR="$PWD"
else
    CURDIR="$(readlink -e "$(pwd)")"
fi

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
                        has_match=y
                        [[ "$Z_LIST_SKIP" =~ "C" ]] && continue
                        echo "${f#$CURDIR/}"
                        continue
                        ;;
                    *testem.json)
                        test -f "$f" || continue
                        has_match=y
                        [[ "$Z_LIST_SKIP" =~ "web" ]] && continue
                        ;;
                    *.py)
                        test -f "$f" || continue
                        has_match=y
                        [[ "$Z_LIST_SKIP" =~ "C" ]] && continue
                        ;;
                    *)
                        test -x "$f" || continue
                        has_match=y
                        [[ "$Z_LIST_SKIP" =~ "C" ]] && continue
                        ;;
                esac

                echo "$zd/ $line"
            done
            test -n "$has_match" || err "$zf:$lno: no match for $line"
            IFS="$ifs"
        done
        exit $_err
    ) < "$zf"
}

find "$1" -type f -not \( -path "$Z_SKIP_PATH" \) -name ZFile | (
    while read zf; do
        dump_zf "$zf" || _err=1
    done
    exit $_err
)

exit $_err
