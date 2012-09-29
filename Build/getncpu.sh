#!/bin/sh

case "$PATH" in
    *distcc*)
        res=$(distcc -j 2> /dev/null)
        if [ -n "$res" ]; then
            expr $res + 1
            exit 0
        fi
        ;;
    *)
        ;;
esac
expr $(getconf _NPROCESSORS_ONLN) + 1
