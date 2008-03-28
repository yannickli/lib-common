/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "str-path.h"

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
    /* OG: this function should not return NULL, it should return a
     * pointer to the end of the filename if it does not have an
     * extension.
     */
    return lastdot;
}
