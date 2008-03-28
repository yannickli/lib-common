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

/****************************************************************************/
/* simple file name splits                                                  */
/****************************************************************************/

/* MX: XXX: does not work like the usual libgen `basename`
 *
 * basename("foo////") == "foo" the rightmost '/' are not significant
 * basename("////") == "/"
 */
const char *path_filepart(const char *filename)
{
    const char *base = filename;
    for (;;) {
        filename = pstrchrnul(filename, '/');
        if (!*filename)
            return base;
        base = ++filename;
    }
}

ssize_t path_dirpart(char *dir, ssize_t size, const char *filename)
{
    return pstrcpylen(dir, size, filename, path_filepart(filename) - filename);
}

const char *path_ext(const char *filename)
{
    const char *base = path_filepart(filename);
    const char *lastdot = NULL;

    while (*base == '.') {
        base++;
    }
    for (;;) {
        base = pstrchrnul(base, '.');
        if (!*base)
            return lastdot;
        lastdot = base++;
    }
}

const char *path_extnul(const char *filename)
{
    const char *base = path_filepart(filename);
    const char *lastdot = NULL;

    while (*base == '.') {
        base++;
    }
    for (;;) {
        base = pstrchrnul(base, '.');
        if (!*base)
            return lastdot ? lastdot : base;
        lastdot = base++;
    }
}

/****************************************************************************/
/* libgen like functions                                                    */
/****************************************************************************/

int path_dirname(char *buf, int len, const char *path)
{
    const char *end = path + strlen(path);

    while (end > path && end[-1] == '/')
        --end;
    while (end > path && end[-1] != '/')
        --end;
    while (end > path && end[-1] == '/')
        --end;
    if (end > path)
        return pstrcpylen(buf, len, path, end - path);
    if (*path == '/')
        return pstrcpy(buf, len, path[1] == '/' ? "//" : "/");
    return pstrcpy(buf, len, ".");
}

int path_basename(char *buf, int len, const char *path)
{
    for (;;) {
        const char *end = pstrchrnul(path, '/');
        const char *p = end;
        while (*p == '/')
            p++;
        if (!*p)
            return pstrcpylen(buf, len, path, end - path);
        path = p;
    }
}

/****************************************************************************/
/* path manipulations                                                       */
/****************************************************************************/

int path_join(char *buf, int len, const char *path)
{
    int pos = strlen(buf);
    int min = buf[0] == '/' && buf[1] == '/';

    while (pos > min && buf[pos - 1] == '/')
        --pos;
    while (*path == '/')
        path++;
    pos += pstrcpy(buf + pos, len - pos, "/");
    pos += pstrcpy(buf + pos, len - pos, path);
    return pos;
}
