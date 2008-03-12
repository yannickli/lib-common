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
    farch_entry_hash h;
    int use_dir;
    char dir[PATH_MAX];
};

farch_t *farch_new(const farch_entry_t files[], const char *overridedir)
{
    struct stat st;
    farch_t *fa = p_new(farch_t, 1);

    farch_entry_hash_init(&fa->h);
    while (files->name) {
        farch_entry_hash_insert2(&fa->h, (farch_entry_t *)files++);
    }
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
int farch_get(const farch_t *fa, blob_t *buf, const byte **data, int *size,
              const char *name)
{
    farch_entry_t *ent;

    if (fa->use_dir) {
        char fname[PATH_MAX];
        snprintf(fname, sizeof(fname), "%s/%s", fa->dir, name);
        if (!fname[0]) {
            return -1;
        }
        if (blob_append_file_data(buf, fname) >= 0) {
            *data = buf->data;
            *size = buf->len;
            return 0;
        }
        /* Could not read from dir. Fall back to embedded data. */
    }
    ent = farch_entry_hash_get2(&fa->h, name);
    if (ent) {
        *data = ent->data;
        *size = ent->size;
        return 0;
    }
    return -1;
}
