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

missing_dep=
profile=
out=/dev/stdout

# {{{ library

log_() {
    echo -e 2>&1 "$@"
}

die() {
    log_ "\e[1;31m"'/!\' "$@" '/!\'"\\\e[0m"
    exit 1
}

log() {
    log_ "..." "$@"
}

warn() {
    log_ "***" "$@"
    missing_dep=t
}

setenv() {
    v="$1"
    shift
    echo "export $v" "=" "$@" >> "$out"
}

setvar() {
    v="$1"
    shift
    echo "$v" "=" "$@" >> "$out"
}

setvardef() {
    setenv $*
    echo "CFLAGSBASE += -D$1=$2" >> "$out"
}

file_exists() {
    test -e "$1" -a -r "$1"
}

check_tool() {
    which "$1" >/dev/null 2>&1 || warn "$1 is missing, apt-get install $2"
}

pkg_config_setvar() {
    pfx="$1"; shift
    deb="$1"; shift


    while test $# != 0; do
        pkg="$1"
        if pkg-config --print-errors "$pkg"; then
            setvar "${pfx}_CFLAGS" "$(pkg-config --cflags "$pkg")"
            setvar "${pfx}_LIBS"   "$(pkg-config --libs "$pkg")"
            return 0
        fi
        shift
    done
    warn "$deb is missing (apt-get install $deb)"
}

prereq() {
    want="$1"
    has="$2"

    test "$want" = "$has" && return 0
    while test -n "$want"; do
        test -z "$has" && return 1
        w="${want%%.*}"
        h="${has%%.*}"
        case "$want" in *.*) want="${want#*.}";; *) want=""; esac
        case "$has" in *.*)  has="${has#*.}";;   *) has=""; esac

        test "$w" -lt "$h" && return 0
        test "$w" -gt "$h" && return 1
    done

    return 0
}

check_iopc() {
    setenv "IOPVER" "-5"
    setenv "IOPFLAGS" "--Wextra --c-export-symbols --c-export-nullability --c-resolve-includes --check-snmp-table-has-index --c-unions-use-enums --c-minimal-includes"
}

__check_python_mod() {
    mod="$1"
    msg="$2"

    if ! $python2_bin -c "import $mod" 2> /dev/null; then
        warn "python module $mod is missing$msg"
    fi
}

check_python_mod_pip() {
    __check_python_mod "$1" " (pip install $2)"
}

check_python_mod_tools() {
    __check_python_mod "$1" ", update your tools"
}

# }}}

while test $# != 0; do
    case "$1" in
        -p)
            shift
            test $# != 0 || die "missing -p argument"
            profile="$1"
            shift
            ;;
        -o)
            shift
            test $# != 0 || die "missing -o argument"
            out="$1"
            shift
            ;;
        *) die "unknown argument $1";;
    esac
done

log "start configure"
test -n "$out" -a "$out" != "/dev/stdout" && rm -f "$out"


if test -n "$profile" -a -d /opt/intersec/${profile}; then
    export BUILDFOR_PREFIX="/opt/intersec/${profile}"
    export PKG_CONFIG_LIBDIR="${BUILDFOR_PREFIX}/usr/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="/usr/lib64/pkgconfig:${PKG_CONFIG_LIBDIR}"
    export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1
    export PKG_CONFIG_ALLOW_SYSTEM_LIBS=1
else
    export PKG_CONFIG_LIBDIR="/usr/lib64/pkgconfig:$(pkg-config --variable pc_path pkg-config)"
fi

# {{{ tools

for tool in clang clang++ flex gperf xsltproc; do
    check_tool $tool $tool
done

# }}}
# {{{ pkg-config packages

pkg_config_setvar "libxml2" "libxml2-dev"    "libxml-2.0"
pkg_config_setvar "zlib"    "zlib1g-dev"     "zlib"
pkg_config_setvar "openssl" "libssl-dev"     "openssl"

pkg_config_setvar "valgrind" "valgrind" "valgrind"

# }}}
# Python {{{

read_py2ver() {
    pyver="$(python -V 2>&1)"
    pyver="${pyver#Python }"
    pyver_major="${pyver:0:1}"
    pyver_minor="${pyver:2:1}"
}

python2_bin="python"
read_py2ver

case "$pyver_major" in
    "2")
        python2_bin="python"
        python3_bin="python3"
        ;;
    "3")
        python2_bin="python2"
        python3_bin="python"
        read_py2ver
        ;;
esac

check_tool $python2_bin "python"

# check that python2 is at least a 2.6
if [ -n "$pyver_minor" ] && [ "$pyver_minor" -lt 6 ]; then
    if which python2.7-config &> /dev/null; then
        python2_bin="python2.7"
    elif which python2.6-config &> /dev/null; then
        python2_bin="python2.6"
    else
        warn "python >= 2.6 headers are missing, apt-get install python-dev"
    fi
fi

setenv python2_BIN $python2_bin
setenv python3_BIN $python3_bin

if which "${python2_bin}-config" &> /dev/null; then
    python2_ENABLE=1
    setenv python2_ENABLE  1
    setenv python2_CFLAGS  "$(${python2_bin}-config --includes)"
    setenv python2_LIBS    "$(${python2_bin}-config --ldflags)"
fi

if which "${python3_bin}-config" &> /dev/null; then
    python3_ENABLE=1
    setenv python3_ENABLE  1
    setenv python3_CFLAGS  "$(${python3_bin}-config --includes)"
    setenv python3_LIBS    "$(${python3_bin}-config --ldflags)"
    setenv python3_SUFFIX  "$(${python3_bin}-config --extension-suffix)"
fi

if [ -z "${python2_ENABLE}" ] && [ -z "${python3_ENABLE}" ]; then
    warn "python headers are missing, apt-get install python-dev"
fi

# }}}
# {{{ linux uapi sctp header

if test -r /usr/include/linux/sctp.h; then
    setvardef "HAVE_LINUX_UAPI_SCTP_H" "1"
else
    log "missing linux uapi sctp header, it will be replaced by a custom one"
fi

# }}}
# {{{ npm

# Since npm 5.7, a 'npm ci' command is provided that does not
# modify the package-lock.json.
# For simplification, just the major version is checked, so
# for npm >= 6.0.0, npm ci is used
if [ $(npm --version | cut -d. -f1) -ge 6 ]; then
    setenv npminstall_BIN "npm ci"
else
    setenv npminstall_BIN "npm install"
fi

# }}}
