#!/bin/sh -e

cc="$1"
version=$("$cc" -dumpversion)
clang_version="$("$cc" --version | grep 'clang version' | cut -d ' ' -f 3)"

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

gcc_prereq()
{
    case "$cc" in
        cc*|gcc*|c++*|g++*) ;;
        *) return 1;
    esac
    prereq "$1" "$version"
}

is_clang()
{
    case "$cc" in
        clang*|c*-analyzer) return 0;;
        *) return 1;;
    esac
}

clang_prereq()
{
    is_clang || return 1
    prereq "$1" "$clang_version"
}

is_cpp()
{
    case "$cc" in
        *++*) return 0;;
        *) return 1;;
    esac
}

getppid()
{
    read pid comm state ppid rest < /proc/$1/stat
    echo $ppid
}

from_editor()
{
    test -f /proc/self/exe || return 1
    pid=$(getppid self)
    while test "$pid" != 1; do
        case "$(readlink /proc/$pid/exe)" in
            *vim*|*emacs*)
                return 0;;
            *)
                pid=$(getppid $pid);;
        esac
    done
    return 1
}

# use C99 to be able to for (int i =...
( is_cpp && echo -std=gnu++98 ) || echo -std=gnu99
# optimize even more
echo -O2

if is_clang; then
    if $(from_editor); then
        echo -fno-caret-diagnostics
    fi
    if test "$2" != "rewrite"; then
        echo -fdiagnostics-show-category=name
    fi
    echo "-fblocks"
else
    echo -funswitch-loops
    # ignore for (i = 0; i < limit; i += N) as dangerous for N != 1.
    echo -funsafe-loop-optimizations
    echo -fshow-column
    if gcc_prereq 4.3; then
        echo -fpredictive-commoning
        echo -ftree-vectorize
        echo -fgcse-after-reload
    fi
fi
# know where the warnings come from
echo -fdiagnostics-show-option

if test "$2" != "rewrite"; then
    # let the type char be unsigned by default
    echo -funsigned-char
    # do not use strict aliasing, pointers of different types may alias.
    echo -fno-strict-aliasing
fi
# let overflow be defined
echo -fwrapv

# turn on all common warnings
echo -Wall
echo -Wextra

# treat warnings as errors but for a small few
echo -Werror
if gcc_prereq 4.3 || is_clang; then
    echo -Wno-error=deprecated-declarations
else
    echo -Wno-deprecated-declarations
fi
if gcc_prereq 4.6; then
    echo -Wno-error=unused-but-set-variable
    cat <<EOF | $cc -c -x c -o /dev/null - >/dev/null 2>/dev/null || echo -D__gcc_has_no_ifunc
static int foo(void) { };
void (*bar(void))(void) __attribute__((ifunc("foo")));
EOF

fi
if is_clang; then
    echo -Wno-gnu-designator
    echo -Wno-return-type-c-linkage
fi

echo -Wchar-subscripts
# warn about undefined preprocessor identifiers
echo -Wundef
# warn about local variable shadowing another local variable
echo -Wshadow -D'"index(s,c)=index__(s,c)"'
# make string constants const
echo -Wwrite-strings
# warn about implicit conversions with side effects
# fgets, calloc and friends take an int, not size_t...
#echo -Wconversion
# warn about comparisons between signed and unsigned values, but not on
echo -Wsign-compare
# warn about unused declared stuff
echo -Wunused
# do not warn about unused function parameters
echo -Wno-unused-parameter
# warn about variable use before initialization
echo -Wuninitialized
# warn about variables which are initialized with themselves
echo -Winit-self
if gcc_prereq 4.5; then
    echo -Wenum-compare
    echo -Wlogical-op
fi
if gcc_prereq 4 6; then
    echo -flto -fuse-linker-plugin
fi
# warn about pointer arithmetic on void* and function pointers
echo -Wpointer-arith
# warn about multiple declarations
echo -Wredundant-decls
if gcc_prereq 4.2; then
    # XXX: this is disabled for 4.1 because we sometimes need to disable it
    #      and GCC diagostic pragmas is a 4.2 feature only. As gcc-4.1 is no
    #      longer used for development this isn't a problem.
    # warn if the format string is not a string literal
    echo -Wformat-nonliteral
fi
# do not warn about strftime format with y2k issues
echo -Wno-format-y2k
# warn about functions without format attribute that should have one
echo -Wmissing-format-attribute
# barf if we change constness
#echo -Wcast-qual

if is_cpp; then
    if test "$2" != "rewrite"; then
        echo -fno-rtti
        echo -fno-exceptions
    fi
    if gcc_prereq 4.2; then
        echo -Wnon-virtual-dtor
        echo -Woverloaded-virtual
    else
        echo -Wno-shadow
    fi
else
    # warn about functions declared without complete a prototype
    echo -Wstrict-prototypes
    echo -Wmissing-prototypes
    echo -Wmissing-declarations
    # warn about extern declarations inside functions
    echo -Wnested-externs
    # warn when a declaration is found after a statement in a block
    echo -Wdeclaration-after-statement
    # do not warn about zero-length formats.
    echo -Wno-format-zero-length
fi

echo -D_GNU_SOURCE $(getconf LFS_CFLAGS)

if test "$2" = rewrite; then
    get_internal_clang_args()
    {
        while test $# != 0; do
            case "$1" in
                '"'-internal-*)
                    echo $1
                    echo $2
                    shift 2
                    ;;
                *)
                    shift
                    ;;
            esac
        done
    }

    get_internal_clang_args $(clang -x c${cc#clang} -'###' /dev/null 2>&1 | grep 'cc1')
fi
