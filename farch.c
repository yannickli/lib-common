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

#include "container.h"
#include "farch.h"

qm_kptr_t(fe, const char, const farch_entry_t *,
          qhash_str_hash, qhash_str_equal);

struct farch_t {
    flag_t   use_dir     : 1;
    flag_t   checked_dir : 1;
    qm_t(fe) h;
    char     dir[PATH_MAX];
};

farch_t *farch_new(const farch_entry_t files[], const char *overridedir)
{
    farch_t *fa = p_new(farch_t, 1);

    qm_init(fe, &fa->h, true);
    fa->use_dir = overridedir && *overridedir;
    if (fa->use_dir) {
        pstrcpy(fa->dir, sizeof(fa->dir), overridedir);
    }
    farch_add(fa, files);
    return fa;
}

void farch_add(farch_t *fa, const farch_entry_t files[])
{
    for (; files->name; files++) {
        qm_add(fe, &fa->h, files->name, files);
    }
}

void farch_delete(farch_t **fap)
{
    if (*fap) {
        qm_wipe(fe, &(*fap)->h);
        p_delete(fap);
    }
}

const farch_entry_t *farch_find(const farch_t *fa, const char *name)
{
    return fa->h.values[RETHROW_NP(qm_find_safe(fe, &fa->h, name))];
}


/* Get data back in data/size.
 * If we have to allocate it, use the provided sb.
 */
int farch_get(farch_t *fa, sb_t *buf, const byte **data, int *size,
              const char *name)
{
    const farch_entry_t *ent;

    if (fa->use_dir) {
        char fname[PATH_MAX];
        snprintf(fname, sizeof(fname), "%s/%s", fa->dir, name);
        if (sb_read_file(buf, fname) >= 0) {
            *data = (const byte *)buf->data;
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
    ent = farch_find(fa, name);
    if (ent) {
        *data = ent->data;
        *size = ent->size;
        return 0;
    }
    return -1;
}
