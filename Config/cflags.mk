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

$!clang-flags.mk: $(var/toolsdir)/* $(var/cfgdir)/*
	echo -n "CLANGFLAGS := "                                  >  $@+
	$(var/cfgdir)/cflags.sh "clang" | tr '\n' ' '             >> $@+
	echo                                                      >> $@+
	echo -n "CLANGXXFLAGS := "                                >> $@+
	$(var/cfgdir)/cflags.sh "clang++" | tr '\n' ' '           >> $@+
	echo                                                      >> $@+
	echo -n "CLANGREWRITEFLAGS := "                           >> $@+
	$(var/cfgdir)/cflags.sh "clang" "rewrite" | tr '\n' ' '   >> $@+
	echo                                                      >> $@+
	echo -n "CLANGXXREWRITEFLAGS := "                         >> $@+
	$(var/cfgdir)/cflags.sh "clang++" "rewrite" | tr '\n' ' ' >> $@+
	echo                                                      >> $@+
	$(MV) $@+ $@

$!cc-$(CC)-flags.mk: $(var/toolsdir)/* $(var/cfgdir)/*
	echo -n "CFLAGS := "                          >  $@+
	$(var/cfgdir)/cflags.sh "$(CC)" | tr '\n' ' ' >> $@+
	echo                                          >> $@+
	$(MV) $@+ $@

$!cxx-$(CXX)-flags.mk: $(var/toolsdir)/* $(var/cfgdir)/*
	echo -n "CXXFLAGS := "                         >  $@+
	$(var/cfgdir)/cflags.sh "$(CXX)" | tr '\n' ' ' >> $@+
	echo                                           >> $@+
	$(MV) $@+ $@

-include $!clang-flags.mk
-include $!cc-$(CC)-flags.mk
-include $!cxx-$(CXX)-flags.mk

CFLAGS       += -I$/lib-common/compat -I$/
CXXFLAGS     += -I$/lib-common/compat -I$/
CXXFLAGS     += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS

CLANGFLAGS          += -I$/lib-common/compat -I$/
CLANGREWRITEFLAGS   += -I$/lib-common/compat -I$/
CLANGXXFLAGS        += -I$/lib-common/compat -I$/
CLANGXXFLAGS        += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS
CLANGXXREWRITEFLAGS += -I$/lib-common/compat -I$/
CLANGXXREWRITEFLAGS += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS
