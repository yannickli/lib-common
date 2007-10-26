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

#include <assert.h>
#include "hashtbl.h"

typedef struct hashtbl_entry {
    uint64_t key;
    void *ptr;
} hashtbl_entry;

#define IS_EMPTY(ptr)  ((uintptr_t)(ptr) <= 1)
#define INVALIDATE(t, ptr) ((t)->nr--, ptr = (void *)1)

void hashtbl_wipe(hashtbl_t *t)
{
    p_delete(&t->tab);
}

static void hashtbl_resize(hashtbl_t *t, size_t newsize)
{
    ssize_t oldsize = t->size;
    hashtbl_entry *oldtab = t->tab;

    t->size = newsize;
    t->tab  = p_new(hashtbl_entry, newsize);
    t->nr   = 0;

    for (ssize_t i = 0; i < oldsize; i++) {
        if (!IS_EMPTY(oldtab[i].ptr))
            hashtbl_insert(t, oldtab[i].key, oldtab[i].ptr);
    }
    p_delete(&oldtab);
}

void **hashtbl_find(const hashtbl_t *t, uint64_t key)
{
    size_t size = (size_t)t->size;
    size_t pos  = key % size;
    hashtbl_entry *tab = t->tab;

    if (!t->tab)
        return NULL;

    while (tab[pos].ptr && tab[pos].key != key) {
        if (++pos == size)
            pos = 0;
    }
    return IS_EMPTY(tab[pos].ptr) ? NULL : &tab[pos].ptr;
}

void **hashtbl_insert(hashtbl_t *t, uint64_t key, void *ptr)
{
    size_t size, pos;
    hashtbl_entry *tab;

    if (t->nr >= t->size / 2)
        hashtbl_resize(t, p_alloc_nr(t->size));

    size = (size_t)t->size;
    pos  = key % size;
    tab  = t->tab;

    while (!IS_EMPTY(tab[pos].ptr) && tab[pos].key != key) {
        if (++pos == size)
            pos = 0;
    }
    if (IS_EMPTY(tab[pos].ptr)) {
        t->nr++;
        tab[pos].ptr = ptr;
        tab[pos].key = key;
        return NULL;
    }
    return &tab[pos].ptr;
}

void hashtbl_remove(hashtbl_t *t, void **pp)
{
    assert (!IS_EMPTY(*pp));
    assert ((char*)*pp >= (char*)t->tab);
    assert ((char *)*pp < (char *)(t->tab + t->size));

    INVALIDATE(t, *pp);
    if (8 * (t->nr + 16) < t->size)
        hashtbl_resize(t, 4 * (t->nr + 16));
}

void hashtbl_map(hashtbl_t *t, void (*fn)(void **, void *), void *priv)
{
    hashtbl_entry *e = t->tab;
    hashtbl_entry *end = t->tab + t->size;

    while (e < end) {
        if (!IS_EMPTY(e->ptr)) {
            (*fn)(&e->ptr, priv);
            if (!e->ptr) { /* means removal */
                INVALIDATE(t, e->ptr);
            }
        }
    }
    if (4 * p_alloc_nr(t->nr) < t->size)
        hashtbl_resize(t, 2 * p_alloc_nr(t->nr));
}
