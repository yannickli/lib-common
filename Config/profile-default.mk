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

include $(var/cfgdir)/cflags.mk
FORTIFY_SOURCE?=-D_FORTIFY_SOURCE=2


#Could be of use sometimes, need recent libc though IIRC
ifndef SPARSE
    CFLAGS += $(if $(filter -D_FORTIFY_SOURCE=%,$(ADD_CFLAGS)),,$(FORTIFY_SOURCE))
endif
CFLAGS += -fno-omit-frame-pointer -fvisibility=hidden
CXXFLAGS += -fno-omit-frame-pointer -fvisibility=hidden
LDFLAGS += -Xlinker -export-dynamic

clang-analyzer: __setup_forward
	MAKELEVEL=0 scan-build --use-analyzer $(shell which clang) --use-cc $(shell which clang) --use-c++ $(shell which clang++) $(MAKE)
