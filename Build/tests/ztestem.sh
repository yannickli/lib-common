#!/bin/bash -e
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

SCRIPTDIR=$(dirname "$(readlink -f "$0")")
NODE=$(which node || which nodejs)

test -z $NODE && {
    echo "nodejs is required, apt-get install nodejs or http://nodejs.org" >&2
    exit 1
}

clean_up() {
    errcode=$?
    pkill -f "$(basename $NODE).*phantomjs";
    exit $errcode
}

trap clean_up ERR SIGHUP SIGINT SIGTERM

$NODE $SCRIPTDIR/ztestem.js $@
exit $?
