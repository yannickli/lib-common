/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2016 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_REFCOUNT_H)
#  error "you must include <lib-common/core.h> instead"
#else
#define IS_LIB_COMMON_CORE_REFCOUNT_H

#define REFCNT_NEW(type, pfx)                                             \
    static inline type *pfx##_new(void) {                                 \
        type *res = pfx##_init(p_new_raw(type, 1));                       \
        res->refcnt = 1;                                                  \
        return res;                                                       \
    }

#define REFCNT_DUP(type, pfx)                                             \
    static inline type *pfx##_dup(type *t) {                              \
        t->refcnt++;                                                      \
        return t;                                                         \
    }

#define REFCNT_DELETE(type, pfx)                                          \
    static inline void pfx##_delete(type **tp) {                          \
        if (*tp) {                                                        \
            if (--(*tp)->refcnt > 0) {                                    \
                *tp = NULL;                                               \
            } else {                                                      \
                pfx##_wipe(*tp);                                          \
                p_delete(tp);                                             \
            }                                                             \
        }                                                                 \
    }

#define DO_REFCNT(type, pfx)                                              \
    REFCNT_NEW(type, pfx)                                                 \
    REFCNT_DUP(type, pfx)                                                 \
    REFCNT_DELETE(type, pfx)

#define REFCOUNT_TYPE(type, tpfx, subt, spfx)                             \
    typedef struct {                                                      \
        int refcnt;                                                       \
        subt v;                                                           \
    } type;                                                               \
                                                                          \
    static inline type *tpfx##_new(void) {                                \
        type *res = p_new_raw(type, 1);                                   \
        spfx##_init(&res->v);                                             \
        res->refcnt = 1;                                                  \
        return res;                                                       \
    }                                                                     \
    static inline type *tpfx##_dup(type *t) {                             \
        t->refcnt++;                                                      \
        return t;                                                         \
    }                                                                     \
    static inline void tpfx##_delete(type **tp) {                         \
        if (*tp) {                                                        \
            if (--(*tp)->refcnt > 0) {                                    \
                *tp = NULL;                                               \
            } else {                                                      \
                spfx##_wipe(&(*tp)->v);                                   \
                p_delete(tp);                                             \
            }                                                             \
        }                                                                 \
    }

#endif
