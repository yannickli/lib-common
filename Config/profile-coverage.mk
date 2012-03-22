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

include $(var/cfgdir)/profile-debug.mk
CFLAGS   += -pg -fprofile-arcs -ftest-coverage
CXXFLAGS += -pg -fprofile-arcs -ftest-coverage
LDFLAGS  += -lgcov

coverage::
	lcov --directory $! --base-directory $/ --zerocounters
	MAKELEVEL=0 $(MAKE) P=$(var/profile) NOCHECK=1 all
	-MAKELEVEL=0 $(MAKE) P=$(var/profile) NOCHECK=1 L=1 check
	lcov --directory $! --base-directory $/ --capture --output-file $!lcov.info
