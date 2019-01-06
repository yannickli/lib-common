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

examples_LIBRARIES = tstiop tstiop2
ifeq (,$(TOOLS_REPOSITORY))
none_SHARED_LIBRARIES = zchk-tstiop-plugin zchk-tstiop2-plugin
endif

tstiop_SOURCES =  \
    tstiop.iop                                                               \
    tstiop_inheritance.iop                                                   \
    tstiop_licence.iop                                                       \
    tstiop_backward_compat.iop                                               \
    tstiop_backward_compat_deleted_struct_1.iop                              \
    tstiop_backward_compat_deleted_struct_2.iop                              \
    tstiop_backward_compat_incompatible_struct_1.iop                         \
    tstiop_backward_compat_incompatible_struct_2.iop                         \
    tstiop_backward_compat_iface.iop                                         \
    tstiop_backward_compat_iface_deleted.iop                                 \
    tstiop_backward_compat_iface_deleted_rpc.iop                             \
    tstiop_backward_compat_iface_deleted_rpc_ignored.iop                     \
    tstiop_backward_compat_iface_deleted_rpc_ignored_bin.iop                 \
    tstiop_backward_compat_iface_deleted_rpc_ignored_json.iop                \
    tstiop_backward_compat_iface_incompatible_rpc.iop                        \
    tstiop_backward_compat_iface_incompatible_rpc_ignored.iop                \
    tstiop_backward_compat_iface_incompatible_rpc_ignored_binjson.iop        \
    tstiop_backward_compat_mod.iop                                           \
    tstiop_backward_compat_mod_deleted.iop                                   \
    tstiop_backward_compat_mod_deleted_if.iop                                \
    tstiop_void_type.iop
tstiop_inheritance.iop_IOPVER=-5

tstiop2_SOURCES = tstiop2.iop

zchk-tstiop-plugin_SOURCES = \
	$liop/tstiop-plugin.c \
	$liop/tstiop.a

zchk-tstiop2-plugin_SOURCES = \
	$liop/tstiop2-plugin.c \
	$liop/tstiop2.a

include ../Build/base.mk
