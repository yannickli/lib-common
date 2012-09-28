#!/bin/sh

case "$PATH" in
    *distcc*)
        expr $(distcc -j) + 1
        exit 0
        ;;
    *)
        ;;
esac
expr $(getconf _NPROCESSORS_ONLN) + 1
