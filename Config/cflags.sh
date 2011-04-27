#!/bin/sh -e

cc="$1"
version=$("$cc" -dumpversion)

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
        clang*) return 0;;
        *) return 1;;
    esac
}

is_cpp()
{
    case "$cc" in
        *++*) return 0;;
        *) return 1;;
    esac
}

# use C99 to be able to for (int i =...
( is_cpp && echo -std=gnu++98 ) || echo -std=gnu99
# optimize even more
echo -O2
if gcc_prereq 4.3; then
    echo -fpredictive-commoning
    echo -ftree-vectorize
    echo -fgcse-after-reload
fi
if ! is_clang; then
    echo -funswitch-loops
    # ignore for (i = 0; i < limit; i += N) as dangerous for N != 1.
    echo -funsafe-loop-optimizations
    # turn on extra warnings
    echo -fshow-column
fi
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
# know where the warnings come from
echo -fdiagnostics-show-option
# treat warnings as errors
echo -Werror
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
if is_clang; then
    # do not warn about unused statement value
    echo -Wno-unused-value
else
    # warn about casting of pointers to increased alignment requirements
    echo -Wcast-align
fi
# warn about variable use before initialization
echo -Wuninitialized
# warn about variables which are initialized with themselves
echo -Winit-self
if gcc_prereq 4.5; then
    echo -Wenum-compare
    echo -Wlogical-op
fi
if gcc_prereq 4.6; then
    #echo -flto -fuse-linker-plugin
    echo -Wsuggest-attribute=noreturn
fi
# warn about pointer arithmetic on void* and function pointers
echo -Wpointer-arith
# warn about multiple declarations
echo -Wredundant-decls
# warn if the format string is not a string literal
echo -Wformat-nonliteral
# do not warn about zero-length formats.
is_cpp || echo -Wno-format-zero-length
# do not warn about strftime format with y2k issues
echo -Wno-format-y2k
# warn about functions without format attribute that should have one
echo -Wmissing-format-attribute
# barf if we change constness
#echo -Wcast-qual

if ! is_cpp; then
    # warn about functions declared without complete a prototype
    echo -Wstrict-prototypes
    echo -Wmissing-prototypes
    echo -Wmissing-declarations
    # warn about extern declarations inside functions
    echo -Wnested-externs
    # warn when a declaration is found after a statement in a block
    echo -Wdeclaration-after-statement
fi

if is_cpp && ! is_clang; then
    echo -fno-exceptions
    echo -fno-rtti
    echo -Wnon-virtual-dtor
    echo -Weffc++
    echo -Woverloaded-virtual
fi

echo -D_GNU_SOURCE $(getconf LFS_CFLAGS)
