/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "core.h"

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
        filename = strchrnul(filename, '/');
        if (!*filename)
            return base;
        base = ++filename;
    }
}

ssize_t path_dirpart(char *dir, ssize_t size, const char *filename)
{
    return pstrcpymem(dir, size, filename, path_filepart(filename) - filename);
}

const char *path_ext(const char *filename)
{
    const char *base = path_filepart(filename);
    const char *lastdot = NULL;

    while (*base == '.') {
        base++;
    }
    for (;;) {
        base = strchrnul(base, '.');
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
        base = strchrnul(base, '.');
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
        return pstrcpymem(buf, len, path, end - path);
    if (*path == '/')
        return pstrcpy(buf, len, "/");
    return pstrcpy(buf, len, ".");
}

int path_basename(char *buf, int len, const char *path)
{
    for (;;) {
        const char *end = strchrnul(path, '/');
        const char *p = end;
        while (*p == '/')
            p++;
        if (!*p)
            return pstrcpymem(buf, len, path, end - path);
        path = p;
    }
}

/****************************************************************************/
/* path manipulations                                                       */
/****************************************************************************/

int path_join(char *buf, int len, const char *path)
{
    int pos = strlen(buf);
    while (pos > 0 && buf[pos - 1] == '/')
        --pos;
    while (*path == '/') {
        path++;
    }
    pos += pstrcpy(buf + pos, len - pos, "/");
    pos += pstrcpy(buf + pos, len - pos, path);
    return pos;
}

/*
 * ^/../   -> ^/
 * /+      -> /
 * /(./)+  -> /
 * aaa/../ -> /
 * //+$    -> $
 */
int path_simplify2(char *in, bool keep_trailing_slash)
{
    bool absolute = *in == '/';
    char *start = in + absolute, *out = in + absolute;
    int atoms = 0;

    if (!*in)
        return -1;

    for (;;) {
        const char *p;

#if 0
        e_info("state: `%.*s` \tin: `%s`", (int)(out - (start - absolute)),
               start - absolute, in);
#endif
        while (*in == '/')
            in++;
        if (*in == '.') {
            if (in[1] == '/') {
                in += 2;
                continue;
            }
            if (in[1] == '.' && (!in[2] || in[2] == '/')) {
                in += 2;
                if (atoms) {
                    atoms--;
                    out--;
                    while (out > start && out[-1] != '/')
                        out--;
                } else {
                    if (!absolute) {
                        out = mempcpy(out, "..", 2);
                        if (*in)
                            *out++ = '/';
                    }
                }
                continue;
            }
        }

        in = strchrnul(p = in, '/');
        memmove(out, p, in - p);
        out += in - p;
        atoms++;
        if (!*in)
            break;
        *out++ = '/';
    }

    start -= absolute;
    if (!keep_trailing_slash && out > start && out[-1] == '/')
        --out;
    if (out == start)
        *out++ = '.';
    *out = '\0';
    return out - start;
}

/* TODO: make our own without the PATH_MAX craziness */
int path_canonify(char *buf, int len, const char *path)
{
    char *out = len >= PATH_MAX ? buf : p_alloca(char, PATH_MAX);

    out = RETHROW_PN(realpath(path, out));
    if (len < PATH_MAX)
        pstrcpy(buf, len, out);
    return strlen(out);
}

/* Expand '~/' at the start of a path.
 * Ex: "~/tmp" => "getenv($HOME)/tmp"
 *     "~foobar" => "~foo" (don't use nis to get home dir for user foo !)
 * TODO?: expand all environment variables ?
 */
char *path_expand(char *buf, int len, const char *path)
{
    static char const *env_home = NULL;
    static char const root[] = "/";
    char path_l[PATH_MAX];

    assert (len >= PATH_MAX);

    if (path[0] == '~' && path[1] == '/') {
        if (!env_home)
            env_home = getenv("HOME") ?: root;
        if (env_home == root) {
            path++;
        } else {
            snprintf(path_l, sizeof(path_l), "%s%s", env_home, path + 1);
            path = path_l;
        }
    }
    /* XXX: The use of path_canonify() here is debatable. */
    return path_canonify(buf, len, path) < 0 ? NULL : buf;
}


/* This function checks if the given path try to leave its chroot
 * XXX this do not work with symbolic links in path (but it's a feature ;) )*/
bool path_is_safe(const char *path)
{
    const char *ptr = path;

    /* Get rid of: '/', '../' and '..' */
    if (path[0] == '/') {
        return false;
    } else
    if (path[0] == '.' && path[1] == '.'
    &&  (path[2] == '/' || path[2] == '\0'))
    {
        return false;
    }

    /* Check for `.* '/../' .* | '/..'$` */
    while ((ptr = strchr(ptr, '/')) != NULL) {
        if (ptr[1] == '.' && ptr[2] == '.'
        &&  (ptr[3] == '/' || ptr[3] == '\0'))
        {
            return false;
        }
        ptr++;
    }
    return true;
}
