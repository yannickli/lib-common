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

import z_compatibility

@z_compatibility.metaclass
class test_ClassB(object):
    __metaclass__ = _iopy_register.metaclass
    def user_method(self):
        return self.field2 == 2 and super(test_ClassB, self).user_method()
