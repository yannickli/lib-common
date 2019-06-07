#!/usr/bin/env python
#vim:set fileencoding=utf-8:
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

import sys

if sys.version_info[0] < 3:
    def metaclass(cls):
        return cls

    def u(*args):
        return unicode(*args)

    def b(*args):
        return bytes(*args)
else:
    def metaclass(cls):
        dct = dict(cls.__dict__)
        dct.pop('__dict__', None)
        return cls.__metaclass__(cls.__name__, cls.__bases__, dct)

    def u(obj, *args):
        return str(obj)

    def b(obj, *args):
        return bytes(obj, encoding='utf-8', *args)
