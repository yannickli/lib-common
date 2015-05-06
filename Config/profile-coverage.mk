##########################################################################
#                                                                        #
#  Copyright (C) 2004-2015 INTERSEC SA                                   #
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


www-coverage:: __setup_forward
	$(msg/rm) "$!"
	$(RM) -rf $!*
	$(msg/generate) "$!${REPORT_DIR}"
	mkdir -p $!${REPORT_DIR}
	$(msg/generate) "$!"
	cp -a $/* $!
	find -type d -name "javascript" -not -path '*/\.*' -not -path '*/cache/*' | while read line ; do \
		$(msg/generate) "instrumented directory: $!$${line}"; \
		$(RM) -rf $!$${line}; \
		rand=`date | md5sum | head -c 9` && istanbul instrument --save-baseline --baseline-file $!${REPORT_DIR}/coverage-$${rand}-baseline.json --complete-copy --no-compact -x "**/ext/**" --output $!$${line} $/$${line} ; \
	done
	find -type d -name "ext" -not -path '*/\.*' -not -path '*/cache/*' -path '*/javascript/*' | while read line ; do \
		$(msg/generate) "external lib $!$${line}"; \
		cp -r $/$${line} $!$${line}; \
	done
	server=`find -not -path '*/\.*' -name coverage-server.js` ; node $$server $!${REPORT_DIR} &
	-MAKELEVEL=0 $(MAKE) P=$(var/profile) NOCHECK=1 L=1 www-check
	pkill -f coverage-server
	$(msg/generate) "report in $!${REPORT_DIR}"
	istanbul report --root $!${REPORT_DIR} --dir $!${REPORT_DIR}
