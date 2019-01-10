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

include $(var/cfgdir)/cflags.mk

CFLAGS += -Wno-error -DNDEBUG -D_FORTIFY_SOURCE=1
ifneq (,$(EXPIRATION_DATE))
    CFLAGS += -DEXPIRATION_DATE=$(EXPIRATION_DATE)
endif
ifneq (,$(INTERSECID))
# INTERSECID?=M:W4TF2-KWXGN-34KV5-BGDFD
    CFLAGS += -DINTERSECID=$(INTERSECID)
endif
ifeq (,$(DISABLE_CHECK_TRACE))
    CFLAGS += -DCHECK_TRACE=1
endif
