#!/usr/bin/env python
# -*- coding: utf-8 -*-
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2016 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################
""" Dump the tests from ZFile

This script handles the ZFile content. The output of this script is the
directory based the path argument and the test specified in the Zfile
(ex. "./platform/qkv/ zchk-store").

Some filters are applied based on Z_SKIP_PATH and Z_LIST_SKIP:
 * Z_SKIP_PATH removes the research of ZFile in these paths
 * Z_LIST_SKIP removes a kind of test.
"""

import sys
import os
import re

IS_FILE = os.path.isfile
IS_EXEC = lambda f: IS_FILE(f) and os.access(f, os.X_OK)
GROUPS = [
    ("behave", re.compile(r".*/behave"),     None),
    ("web",    re.compile(r".*testem.json"), IS_FILE),
    ("web",    re.compile(r".*/check_php"),  None),
    ("C",      re.compile(r".*\.py"),        IS_FILE),
    ("C",      re.compile(r".+"),            IS_EXEC)  # default case
]
RE_TAGS = re.compile(r"@([A-Za-z0-9_]+)")
Z_TAG_SKIP = set(os.getenv("Z_TAG_SKIP", "").split())


def dump_zfile(zfile, skipped_groups):
    folder = os.path.dirname(zfile)

    for num, line in enumerate(open(zfile, 'r')):
        line = line.strip()
        test = line.split()[0]
        test_path = os.path.join(folder, test)

        if line.startswith('#'):
            continue

        if set(RE_TAGS.findall(line)) & Z_TAG_SKIP:
            continue

        for group, regex, check in GROUPS:
            if group not in skipped_groups and regex.match(test_path):
                err = None

                if check and not check(test_path):
                    err = "%s:%d: no match for %s" % (zfile, num + 1, line)

                yield (folder, test, err)
                break


def fetch_zfiles(root):
    paths = os.getenv('Z_SKIP_PATH', None)
    skip = None

    if paths:
        skip = re.compile(paths)

    for path, _, files in os.walk(root):
        for f in files:
            if skip and skip.match(path):
                continue
            if f == 'ZFile':
                yield os.path.join(path, f)


def main(root):
    exit_code = 0
    skipped_groups = os.getenv('Z_LIST_SKIP', "").split()

    for zfile in fetch_zfiles(root):
        for folder, line, err in dump_zfile(zfile, skipped_groups):
            if err:
                sys.stderr.write("ERROR: %s\n" % err)
                sys.stderr.flush()
                exit_code = 2
            else:
                print("%s/ %s" % (folder, line))
                sys.stdout.flush()

    sys.exit(exit_code)


if __name__ == "__main__":
    assert len(sys.argv) == 2
    main(sys.argv[1])
