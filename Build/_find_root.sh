#!/bin/bash -e
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2017 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

root="$1"

while [ -L "$root/../Build/base.mk" ]; do
    root="$root/.."
done
echo "$root"
