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
#include <unistd.h>

#include <lib-common/blob.h>
#include <lib-common/mem.h>

#include "robuf.h"
#include "farch.h"

#define FARCH_MAX_HANDLES 64
struct farch {
    const farch_file *files;
    int use_dir;
    char dir[PATH_MAX];
};

farch *farch_new(const farch_file files[], const char *overridedir)
{
    struct stat st;
    farch *fa;

    fa = p_new(farch, 1);
    fa->files = files;
    /* Set dir if and only if overridedir exists and is a directory.
     * Doing this now saves us a lot of calls to open() later on. If the
     * directory is created after program startup, then it has to be
     * restarted. Too bad.
     */
    if (overridedir && !stat(overridedir, &st) && S_ISDIR(st.st_mode)) {
        fa->use_dir = true;
        pstrcpy(fa->dir, sizeof(fa->dir), overridedir);
    } else {
        fa->use_dir = false;
        fa->dir[0] = '\0';
    }
    return fa;
}

void farch_delete(farch **fa)
{
    if (!fa || !*fa) {
        return;
    }
    p_delete(fa);
}

int farch_namehash(const char *str)
{
    int ret = 0x5A3C9643;
    int key;
    int len;

    len = strlen(str);

#define RIGHT_ROTATE_32(val, n) \
    (((unsigned int)(val) >> (n)) | ((unsigned int)(val) << (32 - (n))))
    while (len) {
        key = (unsigned char)(*str);
        ret = RIGHT_ROTATE_32(ret, 5);
        ret ^= key;
        str++;
        len--;
    }
    return ret;
}

int farch_get(const farch *fa, robuf *dst, const char *name)
{
    int namehash;
    const farch_file *files;

    if (!name || !fa || !dst) {
        return 1;
    }
    /* Deal with files overrides */
    if (fa->use_dir) {
        blob_t *out;
        char buf[PATH_MAX];
        out = robuf_make_blob(dst);
        snprintf(buf, sizeof(buf), "%s/%s", fa->dir, name);
        if (blob_append_file_data(out, buf) >= 0) {
            robuf_blob_consolidate(dst);
            return 0;
        }
        /* Could not read from dir. Fall back to embedded data. */
    }
    robuf_reset(dst);
    namehash = farch_namehash(name);
    files = fa->files;
    for (;;) {
        if (!files[0].name) {
            break;
        }
        if (files[0].namehash == namehash
        &&  !strcmp(files[0].name, name)) {
            robuf_make_const(dst, files[0].data, files[0].size);
            return 0;
        }
        files++;
    }
    return 1;
}

/* Get the content of the buffer in a blob, allowing for replacements
 * variables.
 */
int farch_get_withvars(const farch *fa, robuf *dst, const char *name,
                       int nbvars, const char **vars, const char **values)
{
    const byte *p, *end, *p0, *p1, *p2, *p3;
    int i, var_len;
    blob_t *out;
    robuf ldst;

    if (!fa) {
        return 1;
    }
    robuf_init(&ldst);
    if (farch_get(fa, &ldst, name)) {
        robuf_wipe(&ldst);
        return 1;
    }

    /*
     * QWERTY{ $foobar }UIOP
     * ^     ^  ^     ^ ^
     * |     /  |     / \
     * |    /   |    /   \
     * |   /    |   /     \
     * p  p0    p1 p2     p3
    */
    out = robuf_make_blob(dst);
    end = ldst.data + ldst.size;
    for (p = ldst.data; p < end;) {
        p0 = memsearch(p, end - p, "{", 1);
        if (!p0) {
            break;
        }
        p1 = bskipspaces(p0 + 1);
        if (*p1 != '$') {
            blob_append_data(out, p, p1 - p);
            p = p1;
            continue;
        }
        /* skip '$' */
        p1 += 1;
        p2 = p3 = memsearch(p1, end - p1, "}", 1);
        if (!p3) {
            blob_append_data(out, p, p1 - p);
            p = p1;
            continue;
        }
        /* skip '}' */
        p3 += 1;
        while (p2 > p1 && isspace(p2[-1])) {
            p2--;
        }
        var_len = p2 - p1;
        blob_append_data(out, p, p0 - p);
        for (i = 0; i < nbvars; i++) {
            if (!memcmp(p1, vars[i], var_len)
            && vars[i][var_len] == '\0') {
                blob_append_cstr(out, values[i]);
                break;
            }
        }
        p = p3;
    }
    blob_append_data(out, p, end - p);
    robuf_blob_consolidate(dst);
    robuf_wipe(&ldst);
    return 0;
}

