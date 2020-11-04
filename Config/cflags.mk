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

ifneq ($(OS),darwin)
LDFLAGS := -Wl,--as-needed
endif

ifeq ($(OS),darwin)
	CC_BASE  := clang
	CXX_BASE := clang++
else
ifeq ($(filter %-analyzer,$(CC)),)
	CC_BASE  := $(shell basename "$(CC)")
	CXX_BASE := $(shell basename "$(CXX)")
else
	CC_BASE  := clang
	CXX_BASE := clang++
endif
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

CFLAGSBASE  = -I$lcompat -I$l../ -I$/
CFLAGSBASE += -DHAS_LIBCOMMON_REPOSITORY=$(if $(filter T$(var/platform)/%,T$(var/libcommon)),1,0)
CFLAGSBASE += -DHAS_PLATFORM_REPOSITORY=$(if $(filter T$(var/srcdir)/%,T$(var/platform)),1,0)

CFLAGS              += $(CFLAGSBASE)
CXXFLAGS            += $(CFLAGSBASE)
CLANGFLAGS          += $(CFLAGSBASE)
CLANGREWRITEFLAGS   += $(CFLAGSBASE)
CLANGXXFLAGS        += $(CFLAGSBASE)
CLANGXXREWRITEFLAGS += $(CFLAGSBASE)

CXXFLAGS     += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS
CLANGXXFLAGS        += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS
CLANGXXREWRITEFLAGS += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS

ifeq ($(NOASSERT),1)
CFLAGS += -DNDEBUG
CXXFLAGS += -DNDEBUG
OBJECTEXT = .noassert
endif
