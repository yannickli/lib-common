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
# ylint: disable=import-error

import sys

if sys.version_info[0] < 3:
    from .python2 import iopy as _iopy_so
else:
    from .python3 import iopy as _iopy_so

globals().update(_iopy_so.__dict__)

del sys, _iopy_so
