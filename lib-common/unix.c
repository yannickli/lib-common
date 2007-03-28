/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "macros.h"
#include "unix.h"
#include "string_is.h"

/* Returns 0 if directory exists,
 * Returns 1 if directory was created
 * Returns -1 in case an error occured.
 */
int mkdir_p(const char *dir, mode_t mode)
{
    char *p;
    struct stat buf;
    char dir2[PATH_MAX];
    bool needmkdir = 0;

    if (strlen(dir) + 1 > PATH_MAX) {
        return -1;
    }
    pstrcpy(dir2, sizeof(dir2), dir);

    /* Creating "/a/b/c/d" where "/a/b" exists but not "/a/b/c".
     * First find that "/a/b" exists, replacing slashes by \0 (see below)
     */
    while (stat(dir2, &buf) != 0) {
        if (errno != ENOENT) {
            return -1;
        }
        needmkdir = 1;
        p = strrchr(dir2, '/');
        if (p == NULL) {
            goto creation;
        }
        *p = '\0';
    }
    if (!S_ISDIR(buf.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    if (!needmkdir) {
        /* Directory already exists */
        return 0;
    }

  creation:
    /* Then, create /a/b/c and /a/b/c/d : we just have to put '/' where
     * we put \0 in the previous loop. */
    for (;;) {
        if (mkdir(dir2, mode) != 0) {

            /* if dir = "/a/../b", then we do a mkdir("/a/..") => EEXIST,
             * but we want to continue walking the path to create /b !
             */
            if (errno != EEXIST) {
                // XXX: We might have created only a part of the
                // path, and fail now...
                return -1;
            }
        }
        if (!strcmp(dir, dir2)) {
            break;
        }
        p = dir2 + strlen(dir2);
        *p = '/';
    }

    return 1;
}

/* MX: XXX: does not work like the usual libgen `basename`
 *
 * basename("foo////") == "foo" the rightmost '/' are not significant
 * basename("////") == "/"
 *
 * we need to pass a buffer here too.
 * or better, rename these to get_filepart, get_dirpart...
 */
const char *get_basename(const char *filename)
{
    const char *base = filename;

    while (*filename) {
        if (*filename == '/') {
            base = filename + 1;
        }
        filename++;
    }
    return base;
}

ssize_t get_dirname(char *dir, ssize_t size, const char *filename)
{
/* MC: FIXME: does not work for filename == ""
 *            or filename == "<anything without slashes>"
 *            where it should return "."
 *
 * I propose the following implementation:
 */
#if 0
    ssize_t len = sstrlen(filename);

    while (len > 0 && filename[len - 1] == '/')
        len--;

    while (len > 0 && filename[len - 1] != '/')
        len--;

    while (len > 0 && filename[len - 1] == '/')
        len--;

    if (len)
        return pstrcpylen(dir, size, filename, len);

    if (*filename == '/')
        return pstrcpy(dst, dlen, "/");
    return pstrcpy(dst, dlen, ".");
#else
    return pstrcpylen(dir, size, filename, get_basename(filename) - filename);
#endif
}

const char *get_ext(const char *filename)
{
    const char *base = get_basename(filename);
    const char *lastdot = NULL;

    while (*base == '.') {
        base++;
    }
    while (*base) {
        if (*base == '.') {
            lastdot = base;
        }
        base++;
    }
    return lastdot ? lastdot : base;
}

#if 0
#include <stdio.h>
int main(int argc, char **argv)
{
    int ret;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s DIR\n", argv[0]);
        return 1;
    }
    ret = mkdir_p(argv[1], 0750);
    printf("ret:%d\n", ret);
    return 0;
}
#endif
