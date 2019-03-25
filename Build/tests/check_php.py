#!/usr/bin/env python
# encoding: utf-8
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

"""
check_php is a small utility that will check syntax php to avoid
for example use of mix of php 5.4+ languages on 5.3:
please note that current php of buildbot is used so for centos 6, it will
check with php 5.3 and with centos 5.1, php 5.1 will be used.

The script will try to detect if there is GIT submodule in current
directory or below.

If script is launched in mmsx/product, it will check only PHP in
subdirectories.
If script is launch from project toplevel platform, it will check all
PHP files in all its submodules.
"""


from __future__ import print_function
from seven_six import monkey_patch
monkey_patch()
from subprocess import check_output, STDOUT, CalledProcessError
import os
import os.path as osp
import sys
import ipath


def do_check_php(path):
    os.chdir(path)

    files = check_output(['git', 'ls-files', '--', '*.php']).split('\n')
    files = [x for x in files if x and os.path.isfile(x)]
    errors = []
    for entry in files:
        try:
            check_output(['php', '-l', entry], stderr=STDOUT)
        except CalledProcessError as err:
            errors += err.output.split('\n')
    return errors

def check_dir(directory, root_dir):
    to_check = []
    os.chdir(directory)

    for entry in list(ipath.git_submodule_list()):
        entry_fullpath = osp.abspath(entry)
        if entry_fullpath.startswith(root_dir):
            to_check.append((entry_fullpath, entry[len(root_dir)+1:]))
            to_check += check_dir(entry_fullpath, root_dir)
    return to_check


def main():
    if len(sys.argv) > 1:
        root_dir = os.path.abspath(sys.argv[1])
    else:
        root_dir = os.getcwd()
    # convert to realpath (mix of symbolic link with abspath will fail later)
    root_dir = osp.realpath(root_dir)
    os.chdir(root_dir)

    # on Centos 5, it must be launched from toplevel of the working tree
    to_check = check_dir(ipath.git_toplevel(), root_dir)
    to_check.append((root_dir, osp.basename(root_dir)))

    print("1..{0} Check PHP syntax".format(len(to_check)))
    fail = 0
    for i, (root_dir, entry) in enumerate(to_check, 1):
        errors = do_check_php(root_dir)
        if not errors:
            print("{0} pass {1}".format(i, entry))
        else:
            fail += 1
            errors = ["\n: {0}".format(e) for e in errors]
            print("{0} fail {1}{2}".format(i, entry, "".join(errors)))

    fail = 100.0 * fail / len(to_check)
    print("# 0% skipped  {0}% passed {1}% failed".format(
        int(100 - fail), int(fail)))

if __name__ == "__main__":
    main()
