##########################################################################
#                                                                        #
#  Copyright (C) 2004-2008 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

CFLAGS :=
CPPFLAGS :=
LDFLAGS :=

GCCVERSION := $(shell $(CC) -dumpversion)
ifneq (,$(filter 4.%,$(GCCVERSION)))
  GCC4=1
endif

LDFLAGS += -Wl,--warn-common,-x

ifdef GCC4
    # know where the warnings come from
    CFLAGS += -fdiagnostics-show-option
endif
# Use pipes and not temp files.
CFLAGS += -pipe
# use C99 to be able to for (int i =...
CFLAGS += -std=gnu99
# optimize even more
CFLAGS += -O2
# let the type char be unsigned by default
CFLAGS += -funsigned-char
CFLAGS += -fno-strict-aliasing
# turn on all common warnings
CFLAGS += -Wall
# turn on extra warnings
CFLAGS += $(if $(GCC4),-Wextra,-W)
# treat warnings as errors
CFLAGS += -Werror
CFLAGS += -Wchar-subscripts
# warn about undefined preprocessor identifiers
CFLAGS += -Wundef
# warn about local variable shadowing another local variable
CFLAGS += -Wshadow -D"index(s,c)=index__(s,c)"
# warn about casting of pointers to increased alignment requirements
CFLAGS += -Wcast-align
# make string constants const
CFLAGS += -Wwrite-strings
# warn about implicit conversions with side effects
# fgets, calloc and friends take an int, not size_t...
#CFLAGS += -Wconversion
# warn about comparisons between signed and unsigned values, but not on
CFLAGS += -Wsign-compare
# warn about unused declared stuff
CFLAGS += -Wunused
# do not warn about unused function parameters
CFLAGS += -Wno-unused-parameter
# do not warn about unused statement value
#CFLAGS += -Wno-unused-value
# warn about variable use before initialization
CFLAGS += -Wuninitialized
# warn about variables which are initialized with themselves
CFLAGS += $(if $(GCC4),-Winit-self)
# warn about pointer arithmetic on void* and function pointers
CFLAGS += -Wpointer-arith
# warn about multiple declarations
CFLAGS += -Wredundant-decls
# warn if the format string is not a string literal
CFLAGS += -Wformat-nonliteral
# do not warn about zero-length formats.
CFLAGS += -Wno-format-zero-length
# do not warn about strftime format with y2k issues
CFLAGS += -Wno-format-y2k
# warn about functions without format attribute that should have one
CFLAGS += -Wmissing-format-attribute
# barf if we change constness
#CFLAGS += -Wcast-qual

ifndef CPLUSPLUS
  # warn about function call cast to non matching type
  CFLAGS += -Wbad-function-cast
  # warn about functions declared without complete a prototype
  CFLAGS += -Wstrict-prototypes
  CFLAGS += -Wmissing-prototypes
  CFLAGS += -Wmissing-declarations
  # warn about extern declarations inside functions
  CFLAGS += -Wnested-externs
  # warn when a declaration is found after a statement in a block
  CFLAGS += $(if $(GCC4),-Wdeclaration-after-statement)
endif

CFLAGS += -I$/lib-common/compat -I$/

ifneq (,$(filter CYGWIN%,$(UNAME)))
  CFLAGS += -DCYGWIN
endif

ifneq (,$(filter 3.4,$(GCCVERSION)))
  CFLAGS += -fvisibility=hidden
endif
ifneq (,$(filter 4.%,$(GCCVERSION)))
  CFLAGS += -fvisibility=hidden
endif

# POSIX norms we want to support see feature_test_macros(7)
CFLAGS += -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64## lfs
CFLAGS += -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112L#   # all of IEEE 1003.1-2001
CFLAGS += -D_XOPEN_SOURCE=600#                         # XSI extensions
CFLAGS += -D_BSD_SOURCE#                               # strn?casecmp :/
