##########################################################################
#                                                                        #
#  Copyright (C) 2004-2010 INTERSEC SAS                                  #
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
GCCMAJOR   := $(word 1,$(subst ., ,$(GCCVERSION)))
GCCMINOR   := $(word 2,$(subst ., ,$(GCCVERSION)))
GCC_PREREQ=$(shell test $(GCCMAJOR) -lt $1 || test $(GCCMAJOR) = $1 -a $(GCCMINOR) -lt $2 || echo 1)

LDFLAGS += -Wl,--as-needed

# Use pipes and not temp files.
CFLAGS += -pipe
# use C99 to be able to for (int i =...
CFLAGS += -std=gnu99
# optimize even more
CFLAGS += -O2
ifneq (,$(call GCC_PREREQ,4,3))
CFLAGS += -fpredictive-commoning
CFLAGS += -ftree-vectorize
CFLAGS += -fgcse-after-reload
endif
ifneq (clang,$(CC))
CFLAGS += -funswitch-loops
# ignore for (i = 0; i < limit; i += N) as dangerous for N != 1.
CFLAGS += -funsafe-loop-optimizations
endif
# let the type char be unsigned by default
CFLAGS += -funsigned-char
# let overflow be defined
CFLAGS += -fwrapv
# do not use strict aliasing, pointers of different types may alias.
CFLAGS += -fno-strict-aliasing
# turn on all common warnings
CFLAGS += -Wall
ifneq (clang,$(CC))
# turn on extra warnings
CFLAGS += -fshow-column
endif
ifneq (,$(call GCC_PREREQ,4,0))
    CFLAGS += -Wextra
    # know where the warnings come from
    CFLAGS += -fdiagnostics-show-option
else
    CFLAGS += -W
endif
# treat warnings as errors
CFLAGS += -Werror
CFLAGS += -Wchar-subscripts
# warn about undefined preprocessor identifiers
CFLAGS += -Wundef
# warn about local variable shadowing another local variable
CFLAGS += -Wshadow -D"index(s,c)=index__(s,c)"
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
ifneq (clang,$(CC))
# warn about casting of pointers to increased alignment requirements
CFLAGS += -Wcast-align
else
# do not warn about unused statement value
CFLAGS += -Wno-unused-value
endif
# warn about variable use before initialization
CFLAGS += -Wuninitialized
ifneq (,$(call GCC_PREREQ,4,0))
# warn about variables which are initialized with themselves
    CFLAGS += -Winit-self
endif
ifneq (,$(call GCC_PREREQ,4,5))
CFLAGS += -Wenum-compare
CFLAGS += -Wlogical-op
#CFLAGS += -flto #-fuse-linker-plugin
endif
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

# POSIX norms we want to support see feature_test_macros(7)
CFLAGS += -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64## lfs
CFLAGS += -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112L#   # all of IEEE 1003.1-2001
CFLAGS += -D_XOPEN_SOURCE=600#                         # XSI extensions
CFLAGS += -D_GNU_SOURCE
