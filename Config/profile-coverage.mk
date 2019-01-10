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

include $(var/cfgdir)/profile-debug.mk
CFLAGS   += -pg -fprofile-arcs -ftest-coverage
CXXFLAGS += -pg -fprofile-arcs -ftest-coverage
LDFLAGS  += -lgcov

REPORT_DIR=coverage

# Partially copied from http://bind10.isc.org/wiki/TestCodeCoverage
coverage:: __setup_forward
	lcov --directory $! --base-directory $/ --zerocounters
	MAKELEVEL=0 $(MAKE) P=$(var/profile) NOCHECK=1 all
	-MAKELEVEL=0 $(MAKE) P=$(var/profile) NOCHECK=1 L=1 check
	find $! -name "*.gcno" | grep -v '\.pic\.' | while read line ; do \
	    gcda=`echo $$line | sed 's/\.gcno$$/.gcda/'` ; \
	    if [ \! -e $$gcda ]  ; then \
	        touch $$gcda && echo created empty $$gcda; \
	    fi ; \
	done
	lcov --directory $! --ignore-errors gcov,source --base-directory $/ --capture --output-file $!all.info
	lcov  --remove $!all.info '/usr/*' --output $!lcov.info
	rm $!all.info
