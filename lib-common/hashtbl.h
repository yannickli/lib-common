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

#ifndef IS_LIB_COMMON_HASHTBL_H
#define IS_LIB_COMMON_HASHTBL_H

#include "mem.h"

/*
 * Hash tables from that module don't deal with collitions,
 * though wrappers to deal with it using SLISTS are trivial
 */

typedef struct hashtbl_t {
    ssize_t nr, size, ghosts;
    struct hashtbl_entry *tab;
} hashtbl_t;

GENERIC_INIT(hashtbl_t, hashtbl);
void hashtbl_wipe(hashtbl_t *t);

void **hashtbl_find(const hashtbl_t *t, uint64_t key);
void **hashtbl_insert(hashtbl_t *t, uint64_t key, void *);
void hashtbl_remove(hashtbl_t *t, void **);
void hashtbl_map(hashtbl_t *t, void (*fn)(void **, void *), void *);

#define DO_HASHTBL(type, pfx)                                                \
    typedef struct pfx##_hash {                                              \
        ssize_t nr, size;                                                    \
        struct hashtbl_entry *tab;                                           \
    } pfx##_hash;                                                            \
    \
    GENERIC_INIT(pfx##_hash, pfx##_hash);                                    \
    static inline void pfx##_hash_wipe(pfx##_hash *t) {                      \
        hashtbl_wipe((hashtbl_t *)t);                                        \
    }                                                                        \
    \
    static inline type **pfx##_hash_find(pfx##_hash *t, uint64_t key) {      \
        return (type **)hashtbl_find((hashtbl_t *)t, key);                   \
    }                                                                        \
    static inline type **                                                    \
    pfx##_hash_insert(pfx##_hash *t, uint64_t key, type *e) {                \
        return (type **)hashtbl_insert((hashtbl_t *)t, key, e);              \
    }                                                                        \
    \
    static inline void pfx##_hash_remove(pfx##_hash *t, type **e) {          \
        hashtbl_remove((hashtbl_t *)t, (void **)e);                          \
    }                                                                        \
    static inline void                                                       \
    pfx##_hash_map(pfx##_hash *t, void (*fn)(type **, void *), void *p) {    \
        hashtbl_map((hashtbl_t *)t, (void *)fn, p);                          \
    }

#endif /* IS_LIB_COMMON_HASHTBL_H */
