##########################################################################
#                                                                        #
#  Copyright (C) 2004-2012 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

LDFLAGS := -Wl,--as-needed
ifeq (,$(NOCOMPRESS))
ifneq (,$(shell ld --help | grep compress-debug-sections))
    LDFLAGS += -Wl,--compress-debug-sections=zlib
endif
endif

ifeq ($(filter %-analyzer,$(CC)),)
	CC_BASE  := $(shell basename "$(CC)")
	CXX_BASE := $(shell basename "$(CXX)")
else
	CC_BASE  := clang
	CXX_BASE := clang++
endif

CLANG    := $(shell which "clang")
CLANGXX  := $(shell which "clang++")
CC_FULL  := $(shell which "$(CC)")
CXX_FULL := $(shell which "$(CXX)")

$!clang-flags.mk: $(CLANG) $(var/cfgdir)/cflags.sh
	$(RM) $@
	echo -n "CLANGFLAGS := "                      >  $@+
	$(var/cfgdir)/cflags.sh "clang"               >> $@+
	echo                                          >> $@+
	echo -n "CLANGXXFLAGS := "                    >> $@+
	$(var/cfgdir)/cflags.sh "clang++"             >> $@+
	echo                                          >> $@+
	echo -n "CLANGREWRITEFLAGS := "               >> $@+
	$(var/cfgdir)/cflags.sh "clang" "rewrite"     >> $@+
	echo                                          >> $@+
	echo -n "CLANGXXREWRITEFLAGS := "             >> $@+
	$(var/cfgdir)/cflags.sh "clang++" "rewrite"   >> $@+
	echo                                          >> $@+
	$(MV) $@+ $@

$!cc-$(CC_BASE)-flags.mk: $(CC_FULL) $(var/cfgdir)/cflags.sh
	$(RM) $@
	echo -n "CFLAGS := "                          >  $@+
	$(var/cfgdir)/cflags.sh "$(CC)"               >> $@+
	echo                                          >> $@+
	$(MV) $@+ $@

$!cxx-$(CXX_BASE)-flags.mk: $(CXX_FULL) $(var/cfgdir)/cflags.sh
	$(RM) $@
	echo -n "CXXFLAGS := "                        >  $@+
	$(var/cfgdir)/cflags.sh "$(CXX)"              >> $@+
	echo                                          >> $@+
	$(MV) $@+ $@

ifeq ($(filter %clang,$(CC_BASE)),)
include $!clang-flags.mk
endif
include $!cc-$(CC_BASE)-flags.mk
include $!cxx-$(CXX_BASE)-flags.mk

CFLAGS       += -I$/lib-common/compat -I$/
CXXFLAGS     += -I$/lib-common/compat -I$/
CXXFLAGS     += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS

CLANGFLAGS          += -I$/lib-common/compat -I$/
CLANGREWRITEFLAGS   += -I$/lib-common/compat -I$/
CLANGXXFLAGS        += -I$/lib-common/compat -I$/
CLANGXXFLAGS        += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS
CLANGXXREWRITEFLAGS += -I$/lib-common/compat -I$/
CLANGXXREWRITEFLAGS += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS

ifeq ($(NOASSERT),1)
CFLAGS += -DNDEBUG
CXXFLAGS += -DNDEBUG
OBJECTEXT = .noassert
endif
