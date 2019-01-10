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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_TYPES_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_TYPES_H

/* {{{1 Refcount */

#define REFCNT_NEW(type, pfx)                                             \
    __unused__                                                            \
    static inline type *pfx##_new(void) {                                 \
        type *res = pfx##_init(p_new_raw(type, 1));                       \
        res->refcnt = 1;                                                  \
        return res;                                                       \
    }

#define REFCNT_DUP(type, pfx)                                             \
    __unused__                                                            \
    static inline type *pfx##_dup(type *t) {                              \
        t->refcnt++;                                                      \
        return t;                                                         \
    }

#define REFCNT_DELETE(type, pfx)                                          \
    __unused__                                                            \
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
    __unused__                                                            \
    static inline type *tpfx##_new(void) {                                \
        type *res = p_new_raw(type, 1);                                   \
        spfx##_init(&res->v);                                             \
        res->refcnt = 1;                                                  \
        return res;                                                       \
    }                                                                     \
    __unused__                                                            \
    static inline type *tpfx##_dup(type *t) {                             \
        t->refcnt++;                                                      \
        return t;                                                         \
    }                                                                     \
    __unused__                                                            \
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

/* 1}}} */
/* {{{ Optional scalar types */

#define OPT_OF(type_t)     struct { type_t v; bool has_field; }
typedef OPT_OF(int8_t)     opt_i8_t;
typedef OPT_OF(uint8_t)    opt_u8_t;
typedef OPT_OF(int16_t)    opt_i16_t;
typedef OPT_OF(uint16_t)   opt_u16_t;
typedef OPT_OF(int32_t)    opt_i32_t;
typedef OPT_OF(uint32_t)   opt_u32_t;
typedef OPT_OF(int64_t)    opt_i64_t;
typedef OPT_OF(uint64_t)   opt_u64_t;
typedef OPT_OF(int)        opt_enum_t;
typedef OPT_OF(bool)       opt_bool_t;
typedef OPT_OF(double)     opt_double_t;
typedef opt_bool_t         opt__Bool_t;

/** Initialize an optional field. */
#define OPT(val)           { .v = (val), .has_field = true }
/** Initialize an optional field to “absent”. */
#define OPT_NONE           { .has_field = false }
/** Initialize an optional field if `cond` is fulfilled. */
#define OPT_IF(cond, val)  { .has_field = (cond), .v = (cond) ? (val) : 0 }

/** Tell whether the optional field is set or not. */
#define OPT_ISSET(_v)  ((_v).has_field == true)
/** Get the optional field value. */
#define OPT_VAL(_v)    ((_v).v)
#define OPT_DEFVAL(_v, _defval)                       \
    ({ typeof(_v) __v = (_v);                         \
       (__v).has_field ? (__v).v : (_defval); })
#define OPT_GET(_v)  \
    ({ typeof(_v) __v = (_v); __v->has_field ? &__v->v : NULL; })

/** Set the optional field value. */
#define OPT_SET(dst, val)  \
    ({ typeof(dst) *_dst = &(dst); _dst->has_field = true; _dst->v = (val); })
/** Clear the optional field value. */
#define OPT_CLR(dst)   (void)((dst).has_field = false)
/** Set the optional field value if `cond` is fulfilled. */
#define OPT_SET_IF(dst, cond, val) \
    ({ if (cond) {                                         \
           OPT_SET(dst, val);                              \
       } else {                                            \
           OPT_CLR(dst);                                   \
       }                                                   \
    })
/** Clear the optional field value if `cond` is fulfilled. */
#define OPT_CLR_IF(dst, cond) \
    do {                                                   \
        if (cond) {                                        \
            OPT_CLR(dst);                                  \
        }                                                  \
    } while (0)

/* 1}}} */
/* Data Baton {{{ */

/** Type to pass as a generic context.
 */
typedef union data_t {
    void    *ptr;
    uint32_t u32;
    uint64_t u64;
} data_t;

/* }}} */

#endif
