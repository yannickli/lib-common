##########################################################################
#                                                                        #
#  Copyright (C) 2004-2011 INTERSEC SAS                                  #
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

CFLAGS     := $(shell $(var/cfgdir)/cflags.sh "$(CC)") -I$/lib-common/compat -I$/
CXXFLAGS   := $(shell $(var/cfgdir)/cflags.sh "$(CXX)") -I$/lib-common/compat -I$/
CLANGFLAGS := $(shell $(var/cfgdir)/cflags.sh "clang" "rewrite") -I$/lib-common/compat -I$/
CLANGXXFLAGS := $(shell $(var/cfgdir)/cflags.sh "clang++" "rewrite") -I$/lib-common/compat -I$/
