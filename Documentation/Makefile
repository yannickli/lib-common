##########################################################################
#                                                                        #
#  Copyright (C) 2004-2019 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

CLEANFILES = html/ man/

dox:
	doxygen doxygen.cfg
	chmod o+r html/* man/*
	chmod o+x html man
	cp *.png *.css html/

include ../Build/base.mk
