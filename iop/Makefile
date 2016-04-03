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

ifeq (,$(TOOLS_REPOSITORY))
examples_LIBRARIES = tstiop

tstiop_SOURCES = tstiop.iop tstiop_inheritance.iop tstiop_licence.iop        \
                 tstiop_backward_compat.iop                                  \
                 tstiop_backward_compat_deleted_struct_1.iop                 \
                 tstiop_backward_compat_deleted_struct_2.iop                 \
                 tstiop_backward_compat_incompatible_struct_1.iop            \
                 tstiop_backward_compat_incompatible_struct_2.iop
tstiop_inheritance.iop_IOPVER=-5

endif

include ../Build/base.mk
