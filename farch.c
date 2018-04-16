/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
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

void farch_obfuscate(const char *in, int len, int *xor_key, char *out)
{
    int key = rand();

    *xor_key = key;
    lstr_obfuscate(LSTR_INIT_V(in, len), key, LSTR_INIT_V(out, len));
}

static void unobfuscate(const char *in, int len, int xor_key, char *out)
{
    lstr_unobfuscate(LSTR_INIT_V(in, len), xor_key, LSTR_INIT_V(out, len));
}

static lstr_t t_farch_aggregate(const farch_entry_t *entry)
{
    char *contents = t_new_raw(char, entry->compressed_size);
    char *tail = contents;
    int compressed_size = 0;

    for (int i = 0; i < entry->nb_chunks; i++) {
        const farch_data_t *chunk = &entry->data[i];

        compressed_size += chunk->chunk_size;
        if (!expect(compressed_size <= entry->compressed_size)) {
            return LSTR_NULL_V;
        }
        unobfuscate(chunk->chunk, chunk->chunk_size, chunk->xor_data_key,
                    tail);
        tail += chunk->chunk_size;
    }

    if (!expect(compressed_size == entry->compressed_size)) {
        return LSTR_NULL_V;
    }

    return LSTR_INIT_V(contents, entry->compressed_size);
}

char *farch_get_filename(const farch_entry_t *entry, char *name)
{
    if (!entry->name.s) {
        return NULL;
    }
    unobfuscate(entry->name.s, entry->name.len, entry->xor_name_key, name);
    name[entry->name.len] = '\0';
    return name;
}

/** Get compressed farch entry.
 *
 * Get a farch entry by its name in a farch archive.
 *
 * \return  a pointer on the (compressed) farch entry if found,
 *          NULL otherwise.
 */
static const farch_entry_t *farch_get_entry(const farch_entry_t files[],
                                            const char *name)
{
    char real_name[FARCH_MAX_FILENAME];
    int len = strlen(name);

    for (; files->name.len > 0; files++) {
        if (len == files->name.len
        &&  strequal(farch_get_filename(files, real_name), name))
        {
            return files;
        }
    }
    return NULL;
}

/** Unarchive and append a '\0' if needed. */
static lstr_t t_farch_unarchive(const farch_entry_t *entry)
{
    lstr_t res;
    char real_name[FARCH_MAX_FILENAME];

    res = LSTR_INIT_V(t_new_raw(char, entry->size + 1), entry->size + 1);

    do {
        t_scope;
        lstr_t contents;

        contents = t_farch_aggregate(entry); /* (and unobfuscate) */
        if (!contents.s) {
            goto error;
        }
        if (entry->compressed_size == entry->size) {
            memcpy(res.v, contents.s, entry->size);
            res.len = entry->size;
        } else { /* uncompress */
            res.len = qlzo1x_decompress_safe(res.v, entry->size,
                                             ps_initlstr(&contents));
            if (res.len != entry->size) {
                goto error;
            }
        }
    } while (0);

    res.v[res.len] = '\0';

    return res;

  error:
    unobfuscate(entry->name.s, entry->name.len, entry->xor_name_key,
                real_name);
    real_name[entry->name.len] = '\0';
    e_panic("cannot uncompress farch entry `%s`", real_name);
}

lstr_t t_farch_get_data(const farch_entry_t *files, const char *name)
{
    const farch_entry_t *entry = name ? farch_get_entry(files, name) : files;

    return entry ? t_farch_unarchive(entry) : LSTR_NULL_V;
}

lstr_t farch_get_data_persist(const farch_entry_t *files, const char *name)
{
    t_scope;
    const farch_entry_t *entry = name ? farch_get_entry(files, name) : files;
    lstr_t content;
    int pos;

    assert (MODULE_IS_LOADED(farch));

    if (!entry) {
        return LSTR_NULL_V;
    }

    pos = qm_reserve(persisted, &_G.persisted, entry, 0);
    if (pos & QHASH_COLLISION) {
        return _G.persisted.values[pos ^ QHASH_COLLISION];
    }

    content = t_farch_unarchive(entry);
    lstr_persists(&content);
    _G.persisted.values[pos] = content;

    return content;
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
