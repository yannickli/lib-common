#!/usr/bin/env python
#vim:set fileencoding=utf-8:
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2011 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

"""
z

z is a wrapper around unnitest (python 2.7+) or unittest2

To write a "group" of test, write a class deriving from unittest.TestCase
and decorate it with z.ZGroup this way:

    import z

    @z.ZGroup
    class MyTest(z.TestCase):
        pass

Any function in MyTest whose name starts with 'test' will be treated as a
test.

Tests functions can be decorated with any of the unittest usual decorator but
for expectedFailure where @z.ZTodo(reason) must be used instead.

As a conveniency, the z.ZFlags() decorator exists to put flags on a test, it's
a conveniency wrapper around unittest.skip()


At the end of your test, just call z.main() (and not unittest.main()).
if Z_HARNESS is set, then z.main() will run in "Z" mode, else it fallbacks to
unittest.main() allowing all the python unittest module features (such as
running tests as specified on the command line).

Note that 'z' extends the unittest module, so you just need to import z and
use z.* as you would use unittest instead
"""

import sys, os, re, traceback
from functools import wraps

try:
    import unittest2 as u
except ImportError:
    import unittest as u

__all__ = []

def public(sym):
    __all__.append(sym.__name__)
    return sym

class _load_tests:
    """
    _load_tests

    @see ZGroup
    """
    def __init__(self):
        self._groups = []

    def __call__(self, loader, tests, prefix):
        suite = u.TestSuite()
        for g in self._groups:
            suite.addTest(loader.loadTestsFromTestCase(g))
        return suite

    def addGroup(self, group):
        self._groups.append(group)

_groups = []
@public
def ZGroup(cls):
    """
    ZGroup

    This decorator registers a TestCase in the z._groups list (used when
    Z_HARNESS is set) and generates a 'load_tests' function for the module
    where the class lives, for use with unittest.main()
    """
    __import__(cls.__module__)
    m = sys.modules[cls.__module__]
    if getattr(m, 'load_tests', None) is None:
        m.load_tests = _load_tests()
    m.load_tests.addGroup(cls)
    _groups.append(cls)
    return cls

_z_modes = set(os.getenv('Z_MODE', '').split())
_flags   = set(os.getenv('Z_TAG_SKIP', '').split())

@public
def ZFlags(*flags):
    """
    ZFlags

    This decorator is a wrapper around unittest.skip() to implement the
    Z_TAG_SKIP required interface.
    """
    fl = set(flags) & _flags
    if len(fl):
        return u.skip("skipping tests flagged with %s" % (" ".join(fl)))
    return lambda x: x

class _ZTodo(u.case._ExpectedFailure):
    """
    _ZTodo

    horrible hack around the _ExpectedFailure exception so that we can
    piggy-back a reason which expectedFailure sadly lacks.

    Since it changes what is passed to addExpectedFailure(), native unittest
    TestResult classes must be overridden to unpack the addExpectedFailure
    function
    """
    def __init__(self, reason, exc_info):
        # can't use super because Python 2.4 exceptions are old style
        u.case._ExpectedFailure.__init__(self, (reason, exc_info))

@public
def ZTodo(reason):
    """
    ZTodo

    Decorator to use instead of unittest.expectedFailure
    """
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            try:
                func(*args, **kwargs)
            except Exception:
                raise _ZTodo(reason, sys.exc_info())
            raise u.case._UnexpectedSuccess
        return wrapper
    return decorator

class _ZTextTestResult(u.TextTestResult):
    """
    _ZTextTestResult

    replacement of TextTestResult to fixup the _ZTodo hack
    """
    def addExpectedFailure(self, test, err):
        reason, exn = err
        super(_ZTextTestResult, self).addExpectedFailure(test, exn)

class _ZTestResult(u.TestResult):
    """
    _ZTestResult

    TestResult subclass implementing the Z protocol
    Only used when Z_HARNESS is set
    """
    def __init__(self, *args, **kwargs):
        super(_ZTestResult, self).__init__(*args, **kwargs)

    def _putSt(self, what, test, rest = ""):
        tid = test._testMethodName
        sys.stdout.write("%d %s %s" % (self.testsRun, what, tid));
        if len(rest):
            sys.stdout.write(" # ")
            sys.stdout.write(rest)
        sys.stdout.write("\n")

    def _putErr(self, test, err):
        tid = test.id()
        if tid.startswith("__main__."):
            tid = tid[len("__main__."):]
        sys.stdout.write(': $ %s %s\n:\n' % (sys.argv[0], tid))

        exctype, value, tb = err
        lines = traceback.format_exception(exctype, value, tb)
        sys.stdout.write(': ')
        sys.stdout.write('\n: '.join(''.join(lines).split("\n")))
        sys.stdout.write("\n")

    def addSuccess(self, test):
        super(_ZTestResult, self).addSuccess(test)
        self._putSt("pass", test)
        sys.stdout.flush()

    def addError(self, test, err):
        super(_ZTestResult, self).addError(test, err)
        self._putSt("fail", test)
        self._putErr(test, err)
        sys.stdout.flush()

    def addFailure(self, test, err):
        super(_ZTestResult, self).addFailure(test, err)
        self._putSt("fail", test)
        self._putErr(test, err)
        sys.stdout.flush()

    def addSkip(self, test, reason):
        super(_ZTestResult, self).addSkip(test, reason)
        self._putSt("skip", test, reason)
        sys.stdout.flush()

    def addExpectedFailure(self, test, err):
        reason, exn = err
        super(_ZTestResult, self).addExpectedFailure(test, exn)
        self._putSt("todo-fail", test, reason)
        self._putErr(test, exn)
        sys.stdout.flush()

    def addUnexpectedSuccess(self, test):
        super(_ZTestResult, self).addUnexpectedSuccess(test)
        self._putSt("todo-pass", test)
        sys.stdout.flush()

try:
    from unittest2 import *
    __all__ += u.__all__
except ImportError:
    from unittest import *
    __all__ += u.__all__


@public
class TestCase(u.TestCase):
    def zHasMode(self, mode):
        return mode in _z_modes

@public
def expectedFailure(*args, **kwargs):
    """
    overrides the unittest definition so that people won't use it by mistake
    """
    raise Exception("Do not use expectedFailure but ZTodo instead")

@public
def main():
    if os.getenv('Z_HARNESS'):
        u.TextTestRunner.resultclass = _ZTestResult
        loader = u.TestLoader()
        wasSuccessful = True
        for group in _groups:
            s = u.TestSuite(loader.loadTestsFromTestCase(group))
            sys.stdout.write("1..%d %s\n" % (s.countTestCases(), group.__name__))
            sys.stdout.flush()
            wasSuccessful = s.run(_ZTestResult()).wasSuccessful() and wasSuccessful
        sys.exit(not wasSuccessful)
    else:
        u.TextTestRunner.resultclass = _ZTextTestResult
        u.main(module = None)

if __name__ == '__main__':
    u.TextTestRunner.resultclass = _ZTextTestResult
    u.main(module = None)
