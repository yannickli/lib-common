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

#include <lib-common/blob.h>
#include <lib-common/mem.h>

#include "farch.h"

#define FARCH_MAX_HANDLES 64
struct farch {
    int use_dir;
    char dir[PATH_MAX];
};


farch *farch_generic_new(const char *overridedir)
{
    farch *fa;

    fa = p_new(farch, 1);
    if (overridedir) {
        fa->use_dir = true;
        pstrcpy(fa->dir, sizeof(fa->dir), overridedir);
    } else {
        fa->use_dir = false;
        fa->dir[0] = '\0';
    }
    return fa;
}

void farch_generic_delete(farch **fa)
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

int farch_generic_get(const farch_file files[], const char *name,
                      const byte **data, int *size)
{
    const byte *ldata;
    int lsize;
    int namehash;

    if (!data) {
        data = &ldata;
    }
    if (!size) {
        size = &lsize;
    }
    if (!name) {
        return 1;
    }
    namehash = farch_namehash(name);
    for (;;) {
        if (!files[0].name) {
            break;
        }
        if (files[0].namehash == namehash
        &&  !strcmp(files[0].name, name)) {
            *data = files[0].data;
            *size = files[0].size;
            return 0;
        }
        files++;
    }
    *data = NULL;
    *size = 0;
    return 1;
}


/* Get the content of the buffer in a blob, allowing for replacements
 * variables.
 */
int farch_generic_get_withvars(blob_t *out, const farch_file files[],
                               const char *name, int nbvars,
                               const char **vars, const char **values)
{
    const byte *data, *p, *end, *p0, *p1, *p2, *p3;
    int size, i, var_len;

    if (farch_generic_get(files, name, &data, &size)) {
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
    end = data + size;
    for (p = data; p < end;) {
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
    return 0;
}
