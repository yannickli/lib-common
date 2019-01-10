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

ruby_var() {
    echo $($ruby_bin -rrbconfig -e "puts(RbConfig::CONFIG['$1'] || RbConfig::CONFIG['$2'] || '')")
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
    if test "$(uname -s)" = "SunOS"; then
        export PKG_CONFIG_LIBDIR="/opt/csw/lib/pkgconfig:${PKG_CONFIG_LIBDIR}"
    fi
fi

# {{{ tools

for tool in clang clang++ flex gperf mtasc swfmill xsltproc scons ruby python; do
    check_tool $tool $tool
done

if ! test -f /usr/include/sys/capability.h; then
    warn "missing libcap, apt-get install libcap-dev"
elif test -r /usr/include/pcap.h; then
    setvar "libpcap_CFLAGS"
    setvar "libpcap_LIBS" "-lpcap"
    if ! test -r /usr/include/pcap/pcap.h; then
        setenv "libpcap_OUTDATED" "1"
    fi
else
    warn "missing libpcap, apt-get install libpcap-dev"
fi

# }}}
# {{{ pkg-config packages

pkg_config_setvar "libxml2" "libxml2-dev"    "libxml-2.0"
pkg_config_setvar "zlib"    "zlib1g-dev"     "zlib"
pkg_config_setvar "openssl" "libssl-dev"     "openssl"

pkg_config_setvar "imlib2"  "libimlib2-dev" "imlib2"
pkg_config_setvar "pcre"    "libpcre3-dev"  "libpcre"

# }}}
# {{{ libreadline-dev

if test -r /usr/lib/libreadline.so -o /usr/lib64/libreadline.so; then
    setvar "readline_CFLAGS"
    setvar "readline_LIBS" "-lreadline"
else
    warn "missing libreadline, apt-get install libreadline-dev"
fi

# }}}
#{{{ ncurses

if pkg-config ncurses; then
    setvar "ncurses_CFLAGS" "$(pkg-config --cflags ncurses)"
    setvar "ncurses_LIBS"   "$(pkg-config --libs ncurses)"
else
    if [ -r /usr/include/ncurses.h ] && [ -r /usr/include/menu.h ]; then
        setvar "ncurses_CFLAGS" " "
    else
        warn "ncurses headers are missing (apt-get install libncurses5-dev)"
    fi

    #libmenu might be used optionally
    if [ -r /usr/lib/libncurses.so ] && [ -r /usr/lib/libmenu.so ]; then
        setvar "ncurses_LIBS" "-lncurses"
    else
        warn "libncurses.so is missing (apt-get install libncurses5-dev)"
    fi
fi

# }}}
# Ruby {{{

ruby_bin=ruby
for bin in ruby1.9.1 ruby1.8; do
    if which $bin &> /dev/null; then
        ruby_bin=$bin
        break
    fi
done

ruby_hdrdir="$(ruby_var rubyhdrdir topdir)"
ruby_arch="$(ruby_var arch)"
if [ -f "$ruby_hdrdir/ruby.h" ] && [ -d "$ruby_hdrdir/$ruby_arch" ]; then
    # Ruby >= 1.9
    setvar "ruby_CFLAGS" "-I$ruby_hdrdir -I$ruby_hdrdir/$ruby_arch -Wno-strict-prototypes -Wno-redundant-decls -DRUBY_19"
    setvar "ruby_LIBS" "$(ruby_var SOLIBS)"
elif [ -f "$ruby_hdrdir/ruby.h" ]; then
    # Ruby >= 1.8
    setvar "ruby_CFLAGS" "-I$ruby_hdrdir -Wno-strict-prototypes -Wno-redundant-decls -DRUBY_18"
    setvar "ruby_LIBS" "$(ruby_var SOLIBS)"
fi

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
    setenv python2_CFLAGS  "$(${python2_bin}-config --cflags | sed 's/\( \|^\)-[^I][^ ]*//g')"
    setenv python2_LIBS    "$(${python2_bin}-config --ldflags)"
fi

if which "${python3_bin}-config" &> /dev/null; then
    python3_ENABLE=1
    setenv python3_ENABLE  1
    setenv python3_CFLAGS  "$(${python3_bin}-config --cflags | sed 's/\( \|^\)-[^I][^ ]*//g')"
    setenv python3_LIBS    "$(${python3_bin}-config --ldflags)"
    setenv python3_SUFFIX  "$(${python3_bin}-config --extension-suffix)"
fi

if [ -z "${python2_ENABLE}" ] && [ -z "${python3_ENABLE}" ]; then
    warn "python headers are missing, apt-get install python-dev"
fi

# }}}
