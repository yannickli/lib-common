#!/bin/sh -e

case "$PATH" in
    *distcc*)
        if test -n "$DISTCC_HOSTS"; then
            expr $(distcc -j) + 1
            exit 0
        fi
        ;;
    *)
        ;;
esac
expr $(getconf _NPROCESSORS_ONLN) + 1
