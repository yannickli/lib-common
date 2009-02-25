#!/bin/bash -e
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2009 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

if test -f /proc/cpuinfo; then
    n=$(grep -c '^processor' /proc/cpuinfo)
    echo "-j$(expr $n + 1)"
else
    echo "-j -l3"
fi
