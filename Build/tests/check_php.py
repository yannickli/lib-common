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

The script will check PHP syntax in all subdirectories.
"""


from __future__ import print_function
from seven_six import monkey_patch
monkey_patch()
from subprocess import check_output, STDOUT, CalledProcessError
import os
import sys
import fnmatch


def find_php_files(path):
    php_files = []
    for dirname, _, filenames in os.walk(path):
        for filename in fnmatch.filter(filenames, '*.php'):
            php_files.append(os.path.join(dirname, filename))
    return php_files


def main():
    if len(sys.argv) > 1:
        root_dir = os.path.abspath(sys.argv[1])
    else:
        root_dir = os.getcwd()
    # convert to realpath (mix of symbolic link with abspath will fail later)
    root_dir = os.path.realpath(root_dir)
    os.chdir(root_dir)

    files = find_php_files(root_dir)

    print("1..{0} Check PHP syntax".format(len(files)))
    fail = 0
    for i, filename in enumerate(files, 1):
        try:
            check_output(['php', '-l', filename], stderr=STDOUT)
            print("{0} pass {1}".format(i, filename))
        except CalledProcessError as err:
            fail += 1
            print("{0} fail {1}{2}".format(i, filename, err.output))

    fail = 100.0 * fail / len(files)
    print("# 0% skipped  {0}% passed {1}% failed".format(
        int(100 - fail), int(fail)))

if __name__ == "__main__":
    main()
