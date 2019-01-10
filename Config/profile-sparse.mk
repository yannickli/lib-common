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

ifneq ('x86_64','$(shell uname -m)')
    CC := CHECK='sparse -Wall -D__SPARSE__' cgcc
else
    CC := CHECK='sparse -Wall -m64 -D__SPARSE__' cgcc
endif

include $(var/cfgdir)/profile-default.mk
CFLAGS += -Wno-transparent-union
LINEAR := 1
