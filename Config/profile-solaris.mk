##########################################################################
#                                                                        #
#  Copyright (C) 2004-2016 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

PATH:=/opt/intersec/gcc-4.3/bin:$(PATH)
export PATH

CC := gcc

include $(var/cfgdir)/profile-default.mk

CFLAGS:=$(filter-out -fvisibility=%,$(CFLAGS))
CFLAGS += -D__EXTENSIONS__ -march=i586
LDFLAGS += -Wl,-rpath-link,/lib -lxnet
