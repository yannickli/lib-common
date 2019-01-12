#!/bin/sh
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

# Variant of version.sh which generates fakes (and thus constant) versions.
# This is used by the FAKE_VERSIONS mode of waf.

git_rcsid() {
    cat <<EOF
char const $1_id[] =
    "\$Intersec: $1 fake-revision \$";
char const $1_git_revision[] = "$1-fake-revision";
char const $1_git_sha1[] = "$1-fake-sha1";
EOF
}

git_product_version() {
    product="$1"
    cat <<EOF
const char ${product}_git_revision[] = "${product}-fake-revision";
const unsigned ${product}_version_major = 0;
const unsigned ${product}_version_minor = 0;
const unsigned ${product}_version_patchlevel = 0;
const char ${product}_version[] = "${product}-fake-version";
EOF
}

case "$1" in
    "rcsid")           shift; git_rcsid           "$@";;
    "product-version") shift; git_product_version "$@";;
    *);;
esac
