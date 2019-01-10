#!/usr/bin/env bash
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

list_in_path() {
    if [ -e "$1/.git" ] && ! [ -L "$1" ]; then
        for f in $(GIT_DIR="$1/.git" git ls-files); do
            if [ -f "$1/$f" ]; then
                echo "    ,   \"$2$f\""
            fi
        done
    fi
}

TEMP="$(mktemp)"
SRCROOT="$1"
SRCSUBDIR="$2"
PROFILE="$3"

watchman watch-project $1
(
    echo "[ \"trigger\", \"$SRCROOT\", {"
    echo "    \"name\": \"make_watch\","
    echo "    \"command\": [ \"make\", \"P=$PROFILE\", \"NOCHECK=1\", \"-C\","
    echo "                   \"$SRCSUBDIR\", \"full\" ],"
    echo "    \"append_files\": false,"
    echo "    \"stdout\": \">>$TEMP\","
    echo "    \"stderr\": \">>$TEMP\","
    echo "    \"expression\": [ \"name\","
    echo "        [ \".gitignore\""

    list_in_path $SRCROOT
    list_in_path $SRCROOT/platform platform/
    list_in_path $SRCROOT/lib-common lib-common/
    list_in_path $SRCROOT/platform/lib-common platform/lib-common/

    echo "       ],"
    echo "       \"wholename\""
    echo "    ]"
    echo "} ]"
) | watchman -j

trap "watchman trigger-del $SRCROOT make_watch" INT TERM 0
tail -f $TEMP
