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

#ifndef IS_LIB_COMMON_HASHTBL_H
#define IS_LIB_COMMON_HASHTBL_H

#include "mem.h"

/****************************************************************************/
/* simple fast hash tables for non coliding datas                           */
/****************************************************************************/

#define HASHTBLE_TYPE(t)                                                     \
    typedef struct t {                                                       \
        int nr, size, ghosts;                                                \
        struct hashtbl_entry *tab;                                           \
        int name_offs;        /* holds offsetof(type, <indexing field> */    \
        flag_t name_inl : 1;                                                 \
        flag_t inmap : 1;                                                    \
    } t

HASHTBLE_TYPE(hashtbl_t);
GENERIC_INIT(hashtbl_t, hashtbl);
void hashtbl_wipe(hashtbl_t *t);
GENERIC_NEW(hashtbl_t, hashtbl);
GENERIC_DELETE(hashtbl_t, hashtbl);

void **hashtbl_find(const hashtbl_t *t, uint64_t key);
void **hashtbl_insert(hashtbl_t *t, uint64_t key, void *);
void hashtbl_remove(hashtbl_t *t, void **);

/* XXX: modifying `t' from `fn' is totally unsupported and _WILL_ crash */
void hashtbl_map(hashtbl_t *t, void (*fn)(void **, void *), void *);
void hashtbl_map2(hashtbl_t *t, void (*fn)(uint64_t, void **, void *), void *);

#define DO_HASHTBL(type, pfx)                                                \
    HASHTBLE_TYPE(pfx##_hash);                                               \
    \
    GENERIC_INIT(pfx##_hash, pfx##_hash);                                    \
    static inline void pfx##_hash_wipe(pfx##_hash *t) {                      \
        hashtbl_wipe((hashtbl_t *)t);                                        \
    }                                                                        \
    GENERIC_NEW(pfx##_hash, pfx##_hash);                                     \
    GENERIC_DELETE(pfx##_hash, pfx##_hash);                                  \
    \
    static inline type **pfx##_hash_find(pfx##_hash *t, uint64_t key) {      \
        return (type **)hashtbl_find((hashtbl_t *)t, key);                   \
    }                                                                        \
    static inline type *pfx##_hash_get(pfx##_hash *t, uint64_t key) {        \
        type **res = pfx##_hash_find(t, key);                                \
        return res ? *res : NULL;                                            \
    }                                                                        \
    static inline type **                                                    \
    pfx##_hash_insert(pfx##_hash *t, uint64_t key, type *e) {                \
        return (type **)hashtbl_insert((hashtbl_t *)t, key, e);              \
    }                                                                        \
    \
    static inline void pfx##_hash_remove(pfx##_hash *t, type **e) {          \
        hashtbl_remove((hashtbl_t *)t, (void **)e);                          \
    }                                                                        \
    static inline type *pfx##_hash_take(pfx##_hash *t, uint64_t k) {         \
        type **pp = (type **)hashtbl_find((hashtbl_t *)t, k);                \
        type *p; /* stock the elt because the remove fct del memory*/        \
        if (!pp)                                                             \
           return NULL;                                                      \
        p = *pp;                                                             \
        hashtbl_remove((hashtbl_t *)t, (void **)pp);                         \
        return p;                                                            \
    }                                                                        \
    static inline void                                                       \
    pfx##_hash_map(pfx##_hash *t, void (*fn)(type **, void *), void *p) {    \
        hashtbl_map((hashtbl_t *)t, (void *)fn, p);                          \
    }                                                                        \
    static inline void                                                       \
    pfx##_hash_map2(pfx##_hash *t, void (*fn)(uint64_t, type **, void *),    \
                    void *p) {                                               \
        hashtbl_map2((hashtbl_t *)t, (void *)fn, p);                         \
    }


/****************************************************************************/
/* simple hash tables for string_to_element htables                         */
/****************************************************************************/

HASHTBLE_TYPE(hashtbl__t);

uint32_t hsieh_hash(const byte *s, int len);
uint32_t jenkins_hash(const byte *s, int len);
uint64_t combined_hash(const byte *s, int len);

uint64_t hashtbl__hkey(const char *s, int len);
uint64_t hashtbl__hobj(const hashtbl__t *t, void *ptr, int len);
void **hashtbl__find(const hashtbl__t *t, uint64_t key, const char *s);
void **hashtbl__insert(hashtbl__t *t, uint64_t key, void *);

#define HASHTBL_INIT_STR(type, member, inlined) \
    { .name_offs = offsetof(type, name), .name_inl = inlined }
#define HASHTBL_INIT_STROFFS(offs , inlined) \
    { .name_offs = offs, .name_inl = inlined }

#define DO_HASHTBL_OFFS_COMMON(type, pfx)                                    \
    HASHTBLE_TYPE(pfx##_hash);                                               \
    \
    static inline void pfx##_hash_wipe(pfx##_hash *t) {                      \
        hashtbl_wipe((hashtbl_t *)t);                                        \
    }                                                                        \
    GENERIC_DELETE(pfx##_hash, pfx##_hash);                                  \
    \
    static inline uint64_t pfx##_hash_hkey(const char *s, int len) {         \
        return hashtbl__hkey(s, len);                                        \
    }                                                                        \
    static inline uint64_t                                                   \
    pfx##_hash_hobj(pfx##_hash *t, type *ptr, int len) {                     \
        return hashtbl__hobj((hashtbl__t *)t, ptr, len);                     \
    }                                                                        \
    \
    static inline type **                                                    \
    pfx##_hash_find(pfx##_hash *t, uint64_t key, const char *s) {            \
        return (type **)hashtbl__find((hashtbl__t *)t, key, s);              \
    }                                                                        \
    static inline type *                                                     \
    pfx##_hash_get(pfx##_hash *t, uint64_t key, const char *s) {             \
        type **res = pfx##_hash_find(t, key, s);                             \
        return res ? *res : NULL;                                            \
    }                                                                        \
    static inline type **                                                    \
    pfx##_hash_find2(pfx##_hash *t, const char *s) {                         \
        return pfx##_hash_find(t, hashtbl__hkey(s, -1), s);                  \
    }                                                                        \
    static inline type *pfx##_hash_get2(pfx##_hash *t, const char *s) {      \
        type **res = pfx##_hash_find2(t, s);                                 \
        return res ? *res : NULL;                                            \
    }                                                                        \
    static inline type **                                                    \
    pfx##_hash_insert(pfx##_hash *t, uint64_t key, type *e) {                \
        return (type **)hashtbl__insert((hashtbl__t *)t, key, e);            \
    }                                                                        \
    static inline type **pfx##_hash_insert2(pfx##_hash *t, type *e) {        \
        return pfx##_hash_insert(t, pfx##_hash_hobj(t, e, -1), e);           \
    }                                                                        \
    \
    static inline void pfx##_hash_remove(pfx##_hash *t, type **e) {          \
        hashtbl_remove((hashtbl_t *)t, (void **)e);                          \
    }                                                                        \
    static inline type *                                                     \
    pfx##_hash_take(pfx##_hash *t, uint64_t k, const char *s) {              \
        type **pp = (type **)pfx##_hash_find(t, k, s);                       \
        type *p; /* stock the elt because the remove fct del memory */       \
        if (!pp)                                                             \
           return NULL;                                                      \
        p = *pp;                                                             \
        hashtbl_remove((hashtbl_t *)t, (void **)pp);                         \
        return p;                                                            \
    }                                                                        \
    static inline void                                                       \
    pfx##_hash_map(pfx##_hash *t, void (*fn)(type **, void *), void *p) {    \
        hashtbl_map((hashtbl_t *)t, (void *)fn, p);                          \
    }


#define DO_HASHTBL_FIELDOFFS(type, pfx)                                      \
    DO_HASHTBL_OFFS_COMMON(type, pfx)                                        \
    static inline pfx##_hash *pfx##_hash_init_field(pfx##_hash *t, int foff, \
                                                    bool inlined)            \
    {                                                                        \
        p_clear(t, 1);                                                       \
        t->name_offs = foff;                                                 \
        t->name_inl  = inlined;                                              \
        return t;                                                            \
    }                                                                        \
    GENERIC_NEW(pfx##_hash, pfx##_hash);

#define DO_HASHTBL_FIELDINIT(type, pfx, sfx, field, inlined)                 \
    static inline pfx##_hash *pfx##_hash_##sfx##_init(pfx##_hash *t) {       \
        p_clear(t, 1);                                                       \
        t->name_offs = offsetof(type, field);                                \
        t->name_inl  = inlined;                                              \
        return t;                                                            \
    }                                                                        \
    GENERIC_NEW(pfx##_hash, pfx##_hash_##sfx);


/* pass true to `inlined_str` if the ->name member is an inlined array */
#define DO_HASHTBL_STROFFS(type, pfx, offs, inlined_str)                     \
    DO_HASHTBL_OFFS_COMMON(type, pfx)                                        \
                                                                             \
    static inline pfx##_hash *pfx##_hash_init(pfx##_hash *t) {               \
        p_clear(t, 1);                                                       \
        t->name_offs = offs;                                                 \
        t->name_inl  = inlined_str;                                          \
        return t;                                                            \
    }                                                                        \

#define DO_HASHTBL_STR(type, pfx, name, inlined_str)                         \
    DO_HASHTBL_STROFFS(type, pfx, offsetof(type, name), inlined_str)

DO_HASHTBL_STROFFS(char, string, 0, true);
DO_HASHTBL_STROFFS(byte, data, 0, true);


#endif /* IS_LIB_COMMON_HASHTBL_H */
