#!/bin/sh
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2019 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

git_describe() {
    match="$1"
    dirty=$(git diff-files --quiet && git diff-index --cached --quiet HEAD -- || echo "-dirty")
    # the first-parent option is only available since git 1.8
    parent=$(git describe -h 2>&1 | grep 'first-parent' | awk '{ print $1; }')

    if test -n "$match"; then
        echo "$(git describe $parent --match "$match"'*' 2>/dev/null || git rev-parse --short HEAD)${dirty}"
    else
        echo "$(git describe $parent 2>/dev/null || git rev-parse HEAD)${dirty}"
    fi
}

git_rcsid() {
    cat <<EOF
static char const __attribute__((used)) id[] =
    "\$Intersec: $1 $(git_describe | tr -d 'n') \$";
EOF
}

case "$1" in
    "describe") shift; git_describe "$@";;
    "rcsid")    shift; git_rcsid    "$@";;
    *);;
esac
