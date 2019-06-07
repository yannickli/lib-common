# -*- coding: utf-8 -*-
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

def test_add_iop_package(plugin, plugin_r):
    if not issubclass(plugin_r.test_dso.ClassDso, plugin_r.test.ClassA):
        raise AssertionError("ClassDso is not a child class of ClassA")
