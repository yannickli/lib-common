/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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
#include "container-qhash.h"
#include "qlzo.h"

qm_khptr_ckey_t(persisted, farch_entry_t, lstr_t);

static struct {
    qm_t(persisted) persisted;
} farch_g;
#define _G  farch_g

/* {{{ Public API */

const farch_entry_t *farch_get_entry(const farch_entry_t files[],
                                     const char *name)
{
    for (; files->name; files++) {
        if (strequal(files->name, name)) {
            return files;
        }
    }
    return NULL;
}

lstr_t t_farch_get_uncompressed(const farch_entry_t files[],
                                const char *name)
{
    const farch_entry_t *entry = farch_get_entry(files, name);

    if (!entry) {
        return LSTR_NULL_V;
    }

    return t_farch_uncompress(entry);
}

static lstr_t farch_get_uncompressed_persisted(const farch_entry_t *entry)
{
    int pos = qm_find(persisted, &_G.persisted, entry);

    if (pos >= 0) {
        return _G.persisted.values[pos];
    } else {
        return LSTR_NULL_V;
    }
}

lstr_t t_farch_uncompress(const farch_entry_t *entry)
{
    char *res;
    int len;
    lstr_t persisted;

    if (entry->compressed_size == 0) {
        return LSTR_INIT_V(entry->data, entry->size);
    }
    persisted = farch_get_uncompressed_persisted(entry);
    if (persisted.len) {
        return persisted;
    }

    res = t_new_raw(char, entry->size + 1);

    len = qlzo1x_decompress_safe(res, entry->size,
                                 ps_init(entry->data,
                                         entry->compressed_size));
    if (len != entry->size) {
        e_panic("cannot uncompress farch entry `%s`", entry->name);
    }
    res[len] = '\0';

    return LSTR_INIT_V(res, len);
}

lstr_t farch_uncompress_persist(const farch_entry_t *entry)
{
    t_scope;
    lstr_t res;

    assert (MODULE_IS_LOADED(farch));

    if (entry->compressed_size == 0) {
        return LSTR_INIT_V(entry->data, entry->size);
    }
    res = farch_get_uncompressed_persisted(entry);
    if (res.len) {
        return res;
    }

    res = t_farch_uncompress(entry);
    lstr_persists(&res);
    qm_add(persisted, &_G.persisted, entry, res);

    return res;
}

/* }}} */
/* {{{ Module */

static int farch_initialize(void *arg)
{
    qm_init(persisted, &_G.persisted);
    return 0;
}

static int farch_shutdown(void)
{
    qm_deep_wipe(persisted, &_G.persisted, IGNORE, lstr_wipe);
    return 0;
}

MODULE_BEGIN(farch)
MODULE_END()

/* }}} */
