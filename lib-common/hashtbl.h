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

/****************************************************************************/
/* simple fast hash tables for non coliding datas                           */
/****************************************************************************/

#define HASHTBLE_TYPE(t)                                                     \
    typedef struct t {                                                       \
        ssize_t nr, size, ghosts;                                            \
        struct hashtbl_entry *tab;                                           \
        int name_offs;        /* holds offsetof(type, <indexing field> */    \
        flag_t name_inl : 1;                                                 \
        flag_t inmap : 1;                                                    \
    } t

HASHTBLE_TYPE(hashtbl_t);
GENERIC_INIT(hashtbl_t, hashtbl);
void hashtbl_wipe(hashtbl_t *t);

void **hashtbl_find(const hashtbl_t *t, uint64_t key);
void **hashtbl_insert(hashtbl_t *t, uint64_t key, void *);
void hashtbl_remove(hashtbl_t *t, void **);

/* XXX: modifiying `t' from `fn' is totally unsupported and _WILL_ crash */
void hashtbl_map(hashtbl_t *t, void (*fn)(void **, void *), void *);

#define DO_HASHTBL(type, pfx)                                                \
    HASHTBLE_TYPE(pfx##_hash);                                               \
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


/****************************************************************************/
/* simple hash tables for string_to_element htables                         */
/****************************************************************************/

HASHTBLE_TYPE(hashtbl_str_t);

uint32_t hsieh_hash(const byte *s, int len);
uint32_t jenkins_hash(const byte *s, int len);
uint64_t combined_hash(const byte *s, int len);

void **hashtbl_str_find(const hashtbl_str_t *t, uint64_t key,
                        const char *s, int len);
void **hashtbl_str_insert(hashtbl_str_t *t, uint64_t key, void *);

/* pass true to `inlined_str` if the ->name member is an inlined array */
#define DO_HASHTBL_STROFFS(type, pfx, offs, inlined_str)                     \
    HASHTBLE_TYPE(pfx##_hash);                                               \
    \
    static inline void pfx##_hash_init(pfx##_hash *t) {                      \
        p_clear(t, 1);                                                       \
        t->name_offs = offs;                                                 \
        t->name_inl  = inlined_str;                                          \
    }                                                                        \
    static inline void pfx##_hash_wipe(pfx##_hash *t) {                      \
        hashtbl_wipe((hashtbl_t *)t);                                        \
    }                                                                        \
    \
    static inline type **                                                    \
    pfx##_hash_find(pfx##_hash *t, uint64_t key, const char *s, int len) {   \
        return (type **)hashtbl_str_find((hashtbl_str_t *)t, key, s, len);   \
    }                                                                        \
    static inline type **                                                    \
    pfx##_hash_insert(pfx##_hash *t, uint64_t key, type *e) {                \
        return (type **)hashtbl_str_insert((hashtbl_str_t *)t, key, e);      \
    }                                                                        \
    \
    static inline void pfx##_hash_remove(pfx##_hash *t, type **e) {          \
        hashtbl_remove((hashtbl_t *)t, (void **)e);                          \
    }                                                                        \
    static inline void                                                       \
    pfx##_hash_map(pfx##_hash *t, void (*fn)(type **, void *), void *p) {    \
        hashtbl_map((hashtbl_t *)t, (void *)fn, p);                          \
    }

#define DO_HASHTBL_STR(type, pfx, name, inlined_str)                         \
    DO_HASHTBL_STROFFS(type, pfx, offsetof(type, name), inlined_str)

DO_HASHTBL_STROFFS(char, string, 0, true);


#endif /* IS_LIB_COMMON_HASHTBL_H */
