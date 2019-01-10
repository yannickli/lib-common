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

ifeq (,$(NOCOMPRESS))
ifneq (,$(shell ld --help | grep compress-debug-sections))
ifneq (,$(shell objcopy --help | grep compress-debug-sections))
    LDFLAGS += -Xlinker --compress-debug-sections=zlib
endif
endif
endif

ifeq ($(filter %-analyzer,$(CC)),)
	CC_BASE  := $(notdir $(CC))
	CXX_BASE := $(notdir $(CXX))
else
	CC_BASE  := clang
	CXX_BASE := clang++
endif

CLANG    := $(shell which "clang")
CLANGXX  := $(shell which "clang++")
CC_FULL  := $(shell which "$(CC)")
CXX_FULL := $(shell which "$(CXX)")

$!clang-flags.mk: $(CLANG) $(var/cfgdir)/cflags.sh $/configure
	$(RM) $@
	/bin/echo -n "CLANGFLAGS := "                      >  $@+
	$(var/cfgdir)/cflags.sh "clang"               >> $@+
	echo                                          >> $@+
	/bin/echo -n "CLANGXXFLAGS := "                    >> $@+
	$(var/cfgdir)/cflags.sh "clang++"             >> $@+
	echo                                          >> $@+
	/bin/echo -n "CLANGREWRITEFLAGS := "               >> $@+
	$(var/cfgdir)/cflags.sh "clang" "rewrite"     >> $@+
	echo                                          >> $@+
	/bin/echo -n "CLANGXXREWRITEFLAGS := "             >> $@+
	$(var/cfgdir)/cflags.sh "clang++" "rewrite"   >> $@+
	echo                                          >> $@+
	$(MV) $@+ $@

$!cc-$(CC_BASE)-flags.mk: $(CC_FULL) $(var/cfgdir)/cflags.sh $/configure
	$(RM) $@
	/bin/echo -n "CFLAGS := "                          >  $@+
	$(var/cfgdir)/cflags.sh "$(CC)"               >> $@+
	echo                                          >> $@+
	$(MV) $@+ $@

$!cxx-$(CXX_BASE)-flags.mk: $(CXX_FULL) $(var/cfgdir)/cflags.sh $/configure
	$(RM) $@
	/bin/echo -n "CXXFLAGS := "                        >  $@+
	$(var/cfgdir)/cflags.sh "$(CXX)"              >> $@+
	echo                                          >> $@+
	$(MV) $@+ $@

ifeq ($(filter %clang,$(CC_BASE)),)
include $!clang-flags.mk
endif
include $!cc-$(CC_BASE)-flags.mk
include $!cxx-$(CXX_BASE)-flags.mk

CFLAGSBASE += -I$lcompat -I$l../ -I$/
CFLAGSBASE += -DHAS_LIBCOMMON_REPOSITORY=$(if $(filter T$(var/platform)/%,T$(var/libcommon)),1,0)
CFLAGSBASE += -DHAS_PLATFORM_REPOSITORY=$(if $(filter T$(var/srcdir)/%,T$(var/platform)),1,0)

CFLAGS   += $(CFLAGSBASE)
CXXFLAGS += $(CFLAGSBASE)
CXXFLAGS += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS

ifeq ($(filter %clang,$(CC_BASE)),)
CLANGFLAGS          += $(CFLAGSBASE)
CLANGREWRITEFLAGS   += $(CFLAGSBASE)
CLANGXXFLAGS        += $(CFLAGSBASE)
CLANGXXREWRITEFLAGS += $(CFLAGSBASE)
CLANGXXFLAGS        += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS
CLANGXXREWRITEFLAGS += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS
else
CLANGFLAGS          = $(CFLAGS)
CLANGREWRITEFLAGS   = $(CFLAGS)
CLANGXXFLAGS        = $(CXXFLAGS)
CLANGXXREWRITEFLAGS = $(CXXFLAGS)
endif

ifeq ($(NOASSERT),1)
CFLAGS += -DNDEBUG
CXXFLAGS += -DNDEBUG
OBJECTEXT = .noassert
endif
