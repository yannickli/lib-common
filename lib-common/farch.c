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
