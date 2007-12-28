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

/****************************************************************************/
/* simple fast hash tables for non coliding datas                           */
/****************************************************************************/

/* ghosts are continuation placeholder to look for entries that have been put
 * forward before. Their associated key is meaningless and must be disregarded
 */
#define GHOST          ((void *)1)
#define IS_EMPTY(ptr)  ((uintptr_t)(ptr) <= 1)

void hashtbl_wipe(hashtbl_t *t)
{
    assert (!t->inmap);
    p_delete(&t->tab);
}

static void hashtbl_invalidate(hashtbl_t *t, ssize_t pos)
{
    hashtbl_entry *next = &t->tab[pos + 1 == t->size ? 0 : pos + 1];

    t->nr--;
    if (next->ptr) {
        t->ghosts++;
        t->tab[pos].ptr = GHOST;
    } else {
        t->tab[pos].ptr = NULL;
        for (;;) {
            if (pos-- == 0)
                pos = t->size - 1;
            if (t->tab[pos].ptr != GHOST)
                return;
            t->ghosts--;
            t->tab[pos].ptr = NULL;
        }
    }
}

static void hashtbl_resize(hashtbl_t *t, ssize_t newsize)
{
    ssize_t oldsize = t->size;
    hashtbl_entry *oldtab = t->tab;

#ifndef NDEBUG
    switch (CMP(oldsize, newsize)) {
      case CMP_LESS:
        e_trace(2, "growing %p (%zd -> %zd entries)", t, oldsize, newsize);
        break;
      case CMP_EQUAL:
        e_trace(2, "ghosts in %p (%zd entries, %zd ghosts)", t, t->nr,
                t->ghosts);
        break;
      case CMP_GREATER:
        e_trace(2, "shrinking %p (%zd -> %zd entries)", t, oldsize, newsize);
        break;
    }
#endif

    t->size = newsize;
    t->tab  = p_new(hashtbl_entry, newsize);
    t->nr = t->ghosts = 0;

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
    ssize_t ghost = -1;
    hashtbl_entry *tab;

    assert (!t->inmap);
    if (t->nr >= t->size / 2) {
        hashtbl_resize(t, p_alloc_nr(t->size));
    } else
    if (t->nr + t->ghosts >= t->size / 2) {
        hashtbl_resize(t, t->size);
    }

    size = (size_t)t->size;
    pos  = key % size;
    tab  = t->tab;

    while (tab[pos].ptr && tab[pos].key != key) {
        if (tab[pos].ptr == GHOST)
            ghost = pos;
        if (++pos == size)
            pos = 0;
    }
    if (IS_EMPTY(tab[pos].ptr)) {
        if (ghost >= 0) {
            t->ghosts--;
            pos = ghost;
        }
        t->nr++;
        tab[pos].ptr = ptr;
        tab[pos].key = key;
        return NULL;
    }
    return &tab[pos].ptr;
}

void hashtbl_remove(hashtbl_t *t, void **pp)
{
    if (pp) {
        hashtbl_entry *e;

        assert (!t->inmap);
        assert (!IS_EMPTY(*pp));
        e = (hashtbl_entry *)((char *)pp - offsetof(hashtbl_entry, ptr));
        assert (t->tab <= e && e < t->tab + t->size);

        hashtbl_invalidate(t, e - t->tab);
        if (8 * (t->nr + 16) < t->size)
            hashtbl_resize(t, 4 * (t->nr + 16));
    }
}

void hashtbl_map(hashtbl_t *t, void (*fn)(void **, void *), void *priv)
{
    ssize_t pos = t->size;

    assert (!t->inmap);
#ifndef NDEBUG
    t->inmap = true;
#endif

    /* the reverse loop is to maximize the ghosts removal */
    while (pos-- > 0) {
        hashtbl_entry *e = &t->tab[pos];
        if (IS_EMPTY(e->ptr))
            continue;

        (*fn)(&e->ptr, priv);
        if (!e->ptr) { /* means removal */
            hashtbl_invalidate(t, pos);
        }
    }
#ifndef NDEBUG
    t->inmap = false;
#endif
    if (4 * p_alloc_nr(t->nr) < t->size)
        hashtbl_resize(t, 2 * p_alloc_nr(t->nr));
}

void hashtbl_map2(hashtbl_t *t, void (*fn)(uint64_t, void **, void *),
                  void *priv)
{
    ssize_t pos = t->size;

    assert (!t->inmap);
#ifndef NDEBUG
    t->inmap = true;
#endif

    /* the reverse loop is to maximize the ghosts removal */
    while (pos-- > 0) {
        hashtbl_entry *e = &t->tab[pos];
        if (IS_EMPTY(e->ptr))
            continue;

        (*fn)(e->key, &e->ptr, priv);
        if (!e->ptr) { /* means removal */
            hashtbl_invalidate(t, pos);
        }
    }
#ifndef NDEBUG
    t->inmap = false;
#endif
    if (4 * p_alloc_nr(t->nr) < t->size)
        hashtbl_resize(t, 2 * p_alloc_nr(t->nr));
}

#include "property-hash.h"
void props_hash_map(props_hash_t *ph,
                    void (*fn)(const char *, char **, void *), void *priv)
{
    hashtbl_t *t = &ph->h;
    ssize_t pos = t->size;

    assert (!t->inmap);
#ifndef NDEBUG
    t->inmap = true;
#endif

    /* the reverse loop is to maximize the ghosts removal */
    while (pos-- > 0) {
        hashtbl_entry *e = &t->tab[pos];
        if (IS_EMPTY(e->ptr))
            continue;

        (*fn)((const char *)(uintptr_t)e->key, (char **)(void *)&e->ptr, priv);
        if (!e->ptr) { /* means removal */
            hashtbl_invalidate(t, pos);
        }
    }
#ifndef NDEBUG
    t->inmap = false;
#endif
    if (4 * p_alloc_nr(t->nr) < t->size)
        hashtbl_resize(t, 2 * p_alloc_nr(t->nr));
}


/****************************************************************************/
/* simple hash tables for string_to_element htables                         */
/****************************************************************************/

static inline const char *element_name(void *ptr, int offs, bool inl)
{
    void *p = (char *)ptr + offs;
    return inl ? (const char *)p : *(const char **)p;
}

static bool key_equal(const hashtbl__t *t, const hashtbl_entry *e,
                      uint64_t key, const char *s)
{
    const char *name = element_name(e->ptr, t->name_offs, t->name_inl);
    int len = key & 0x7fffffff;
    return !IS_EMPTY(e->ptr) && e->key == key && !memcmp(s, name, len);
}

uint64_t hashtbl__hkey(const char *s, int len)
{
    if (len < 0)
        len = strlen(s);
    return ((uint64_t)hsieh_hash((const byte *)s, len) << 32) | (uint32_t)len;
}

uint64_t hashtbl__hobj(const hashtbl__t *t, void *ptr, int len)
{
    return hashtbl__hkey(element_name(ptr, t->name_offs, t->name_inl), len);
}

void **hashtbl__find(const hashtbl__t *t, uint64_t key, const char *s)
{
    size_t size = (size_t)t->size;
    size_t pos  = key % size;
    hashtbl_entry *tab = t->tab;

    if (!t->tab)
        return NULL;

    while (tab[pos].ptr && !key_equal(t, tab + pos, key, s)) {
        if (++pos == size)
            pos = 0;
    }
    return IS_EMPTY(tab[pos].ptr) ? NULL : &tab[pos].ptr;
}

void **hashtbl__insert(hashtbl__t *t, uint64_t key, void *ptr)
{
    size_t size, pos;
    ssize_t ghost = -1;
    hashtbl_entry *tab;
    const char *name = element_name(ptr, t->name_offs, t->name_inl);

    assert (!t->inmap);
    if (t->nr >= t->size / 2) {
        hashtbl_resize((hashtbl_t *)t, p_alloc_nr(t->size));
    } else
    if (t->nr + t->ghosts >= t->size / 2) {
        hashtbl_resize((hashtbl_t *)t, t->size);
    }

    size = (size_t)t->size;
    pos  = key % size;
    tab  = t->tab;

    while (tab[pos].ptr && !key_equal(t, tab + pos, key, name)) {
        if (tab[pos].ptr == GHOST)
            ghost = pos;
        if (++pos == size)
            pos = 0;
    }
    if (IS_EMPTY(tab[pos].ptr)) {
        if (ghost >= 0) {
            t->ghosts--;
            pos = ghost;
        }
        t->nr++;
        tab[pos].ptr = ptr;
        tab[pos].key = key;
        return NULL;
    }
    return &tab[pos].ptr;
}

/*----- Some useful and very very fast hashes, excellent distribution -----*/

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define get16bits(d) (*((const uint16_t *) (d)))
#else
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       |(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

uint32_t hsieh_hash(const byte *data, int len)
{
    uint32_t hash, tmp;
    int rem;

    if (len < 0)
        len = strlen((const char *)data);

    hash  = len;
    rem   = len & 3;
    len >>= 2;

    /* Main loop */
    for (; len > 0; len--, data += 4) {
        hash  += get16bits(data);
        tmp    = (get16bits(data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
      case 3:
        hash += get16bits(data);
        hash ^= hash << 16;
        hash ^= data[2] << 18;
        hash += hash >> 11;
        break;
      case 2:
        hash += get16bits(data);
        hash ^= hash << 11;
        hash += hash >> 17;
        break;
      case 1:
        hash += *data;
        hash ^= hash << 10;
        hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;
    return hash;
}

uint32_t jenkins_hash(const byte *s, int len)
{
    uint32_t hash = 0;

    if (len < 0) {
        while (*s) {
            hash += *s++;
            hash += hash << 10;
            hash ^= hash >> 6;
        }
    } else {
        while (len-- > 0) {
            hash += *s++;
            hash += hash << 10;
            hash ^= hash >> 6;
        }
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

uint64_t combined_hash(const byte *s, int len)
{
    return ((uint64_t)jenkins_hash(s, len) << 32) | hsieh_hash(s, len);
}
