#!/usr/bin/env python
#vim:set fileencoding=utf-8:
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2012 INTERSEC SAS                                  #
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
import time

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
    def startTest(self, test):
        if self.showAll:
            self.startTime = time.time()
        super(_ZTextTestResult, self).startTest(test)

    def addSuccess(self, test):
        if self.showAll:
            runTime = time.time() - self.startTime
            self.stream.write("[%.3fs] " % (runTime))
        super(_ZTextTestResult, self).addSuccess(test)

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
        tid = getattr(test, '_testMethodName', None)
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

try:
    from behave.formatter.base import Formatter
    from behave.formatter.formatters import register as __behave_register

    class ZFormatter(Formatter):
        """
        Provide a behave formatter that support the z format
        """

        status = {
            "passed":    "pass",
            "failed":    "fail",
            "error":     "fail",
            "skipped":   "skip",
            "untested":  "skip",
            "undefined": "skip",
        }

        name = "z"

        def __init__(self, stream, config):
            self.__count = -1
            self.__success = 0
            self.__skipped = 0
            self.__failed  = 0
            self.__scenario = None
            self.__status   = None
            Formatter.__init__(self, stream, config)

        def flush(self):
            if not self.__scenario is None:
                if self.__status == "pass":
                    self.__success += 1
                elif self.__status == "fail":
                    self.__failed += 1
                elif self.__status == "skip":
                    self.__skipped += 1
                self.stream.write("%d %s %s   # %d steps\n" %
                        (self.__count, self.__status,
                            self.__scenario.name, self.__steps))
                self.__count += 1
            self.__scenario = None
            self.__steps    = 0
            self.__status   = None

        def feature(self, feature):
            self.flush()
            self.stream.write("1..%d %s\n" % (len(feature.scenarios), feature.name))
            self.__count = 1
            self.__steps = 0

        def scenario(self, scenario):
            self.flush()
            self.__scenario = scenario
            self.__status = "skip"

        def result(self, step_result):
            self.__steps += 1
            status = self.status.get(step_result.status, "skip")
            if self.__status == "skip":
                self.__status = status
            elif status == "fail":
                self.__status = "fail"

        def eof(self):
            self.flush()
            if self.__count != 1:
                total = self.__success + self.__skipped + self.__failed
                self.stream.write("# %d%% skipped  %d%% passed  %d%% failed\n" %
                        ((100 * self.__skipped) / total,
                         (100 * self.__success) / total,
                         (100 * self.__failed)  / total))


    def behave_main():
        __behave_register(ZFormatter)
        from behave.__main__ import main as __behave_main
        __behave_main()

except ImportError:
    def behave_main():
        sys.stderr.write("behave not installed\n")

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
        u.main()


group_RE = re.compile(r"^1\.\.(\d+) (.*)$")
test_RE  = re.compile(r"^(\d+) (pass|fail|skip|todo-pass|todo-fail) (.*)$")

def z_report():
    errors = []
    failed_count  = 0
    success_count = 0
    skipped_count = 0

    group_name = ""
    group_len = 0
    group_pos = 0

    last_failed = False
    for line in sys.stdin:
        if line[0] == ':':
            if last_failed:
                error, test, ctx = errors[-1]
                errors[-1] = (error, test, ctx + ":  " + line[1:])
            continue

        line = line.split('#', 1)[0].strip()
        if len(line) == 0:
            continue

        group = group_RE.match(line)
        if group:
            if group_len != group_pos:
                errors.extend([ ("missing", "%s.%d(unknown)" % (group_name, x), '') \
                        for x in xrange(group_pos + 1, group_len) ])
                failed_count += group_len - group_pos
            group_pos = 0
            group_len = int(group.group(1))
            group_name = group.group(2)
            continue

        group = test_RE.match(line)
        if group:
            last_failed = False
            n = int(group.group(1))
            if n < group_pos + 1:
                errors.append(("bad-number", "%s.%s" % (group_name, group.group(3)), ''))
                failed_count += 1
                continue
            elif n > group_pos + 1:
                errors.extend([ ("missing", "%s.%d(unknown)" % (group_name, x), '') \
                        for x in xrange(group_pos + 1, n) ])
                failed_count += n - group_pos - 1
            if group.group(2).endswith('fail'):
                errors.append(("fail", "%s.%s" % (group_name, group.group(3)), ''))
                failed_count += 1
                last_failed   = True
            elif group.group(2) == 'skip':
                skipped_count += 1
            else:
                success_count += 1
            group_pos = n

    total = failed_count + skipped_count + success_count
    if total == 0:
        print "# NO TESTS FOUND"
        sys.exit(0)
    print "#"
    print "#"
    print "# TOTAL"
    print "# Skipped %d%%" % (skipped_count * 100 / total)
    print "# Failed  %d%%" % (failed_count * 100 / total)
    print "# Success %d%%" % (success_count * 100 / total)

    if len(errors):
        print "#"
        print ": ERRORS"
        for error, test, ctx in errors:
            print ": - %s: %s" % (test, error)
            if len(ctx) > 0:
                print ctx
        sys.exit(-1)
    sys.exit(0)


if __name__ == '__main__':
    if os.getenv("Z_BEHAVE"):
        behave_main()
    else:
        z_report()

