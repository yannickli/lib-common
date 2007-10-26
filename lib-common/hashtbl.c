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
    uint64_t h;
    void *ptr;
} hashtbl_entry;

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
        if ((uintmax_t)oldtab[i].ptr > 1)
            hashtbl_insert(t, oldtab[i].h, oldtab[i].ptr);
    }
    p_delete(&oldtab);
}

void **hashtbl_find(const hashtbl_t *t, uint64_t h)
{
    size_t size = t->size;
    size_t pos  = h % size;
    hashtbl_entry *tab = t->tab;

    if (!t->tab)
        return NULL;

    while (tab[pos].ptr && tab[pos].h != h) {
        if (++pos == size)
            pos = size;
    }
    return (uintmax_t)tab[pos].ptr > 1 ? &tab[pos].ptr : NULL;
}

void **hashtbl_insert(hashtbl_t *t, uint64_t h, void *ptr)
{
    size_t size, pos;
    hashtbl_entry *tab;

    if (t->nr >= t->size / 2)
        hashtbl_resize(t, p_alloc_nr(t->size));

    size = t->size;
    pos  = h % size;
    tab  = t->tab;

    while ((uintmax_t)tab[pos].ptr > 1 && tab[pos].h != h) {
        if (++pos == size)
            pos = size;
    }
    if ((uintmax_t)tab[pos].ptr <= 1) {
        t->nr++;
        tab[pos].ptr = ptr;
        tab[pos].h   = h;
        return NULL;
    }
    return &tab[pos].ptr;
}

void hashtbl_remove(hashtbl_t *t, void **pp)
{
    assert ((uintmax_t)*pp >= 1);
    assert ((char*)*pp >= (char*)t->tab);
    assert ((char *)*pp < (char *)(t->tab + t->size));

    *pp = (void *)1;
    t->nr--;
    if (8 * (t->nr + 16) < t->size)
        hashtbl_resize(t, 4 * (t->nr + 16));
}

void hashtbl_map(hashtbl_t *t, void (*fn)(void **, void *), void *priv)
{
    hashtbl_entry *e = t->tab;
    hashtbl_entry *end = t->tab + t->size;

    while (e < end) {
        if ((uintmax_t)e->ptr > 1) {
            (*fn)(&e->ptr, priv);
            if (!e->ptr) { /* means removal */
                e->ptr = (void *)1;
                t->nr--;
            }
        }
    }
    if (8 * (t->nr + 16) < t->size)
        hashtbl_resize(t, 4 * (t->nr + 16));
}
