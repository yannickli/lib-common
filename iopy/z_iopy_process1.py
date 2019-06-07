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

# XXX: python code for a subprocess called from z-iopy.py
# this is to test the listen blocking server mode
# this file contains the client part that connect to the server
# and call the rpc unblocking the server
# arguments:
#    full path name of the iop plugin
#    uri to connect to

import sys
import warnings
import time
import iopy

plugin_file = sys.argv[1]
uri         = sys.argv[2]

p = iopy.Plugin(plugin_file)
r = p.register()

connected = False
t0 = time.time()
while not connected and time.time() - t0 < 30:
    try:
        c = r.connect(uri)
        connected = True
    except iopy.Error as e:
        if str(e) != 'unable to connect to {0}'.format(uri):
            raise e

if not connected:
    sys.exit(100)

# XXX: the server will disconnect inside its RPC impl
warnings.filterwarnings("ignore", category=iopy.Warning,
                        message='.*lost connection.*')

res = c.test_ModuleA.interfaceA.funA(a=r.test.ClassB(field1=1),
                                     _login='root', _password='1234')

c.disconnect()

warnings.resetwarnings()

if res.res != 1:
    sys.exit(101)
sys.exit(0)
