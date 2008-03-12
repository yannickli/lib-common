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

#include "farch.h"
#include "string_is.h"

DO_HASHTBL_STR(farch_entry_t, farch_entry, name, 0);

struct farch_t {
    flag_t use_dir     : 1;
    flag_t checked_dir : 1;
    farch_entry_hash h;
    char dir[PATH_MAX];
};

farch_t *farch_new(const farch_entry_t files[], const char *overridedir)
{
    farch_t *fa = p_new(farch_t, 1);

    farch_entry_hash_init(&fa->h);
    while (files->name) {
        farch_entry_hash_insert2(&fa->h, (farch_entry_t *)files++);
    }
    fa->use_dir = (overridedir && *overridedir);
    if (fa->use_dir) {
        pstrcpy(fa->dir, sizeof(fa->dir), overridedir);
    }
    return fa;
}

void farch_delete(farch_t **fap)
{
    if (*fap) {
        farch_entry_hash_wipe(&(*fap)->h);
        p_delete(fap);
    }
}

/* Get data back in data/size.
 * If we have to allocate it, use the provided blob.
 * */
int farch_get(farch_t *fa, blob_t *buf, const byte **data, int *size,
              const char *name)
{
    farch_entry_t *ent;

    if (fa->use_dir) {
        char fname[PATH_MAX];
        snprintf(fname, sizeof(fname), "%s/%s", fa->dir, name);
        if (blob_append_file_data(buf, fname) >= 0) {
            *data = buf->data;
            *size = buf->len;
            fa->checked_dir = true;
            return 0;
        }
        if (!fa->checked_dir) {
            struct stat st;
            fa->use_dir = !stat(fa->dir, &st) && S_ISDIR(st.st_mode);
            fa->checked_dir = true;
        }
    }
    ent = farch_entry_hash_get2(&fa->h, name);
    if (ent) {
        *data = ent->data;
        *size = ent->size;
        return 0;
    }
    return -1;
}
