/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CONTAINER_H) || defined(IS_LIB_COMMON_CONTAINER_QVECTOR_H)
#  error "you must include container.h instead"
#endif
#define IS_LIB_COMMON_CONTAINER_QVECTOR_H

#define STRUCT_QVECTOR_T(val_t) \
    struct {                         \
        val_t *tab;                  \
        int len, size;               \
        flag_t mem_pool   :  2;      \
        unsigned int skip : 30;      \
    }

typedef STRUCT_QVECTOR_T(uint8_t) qvector_t;

static inline qvector_t *
__qvector_init(qvector_t *vec, void *buf, int blen, int bsize, int mem_pool)
{
    *vec = (qvector_t){
        .tab      = buf,
        .len      = blen,
        .size     = bsize,
        .mem_pool = mem_pool,
    };
    return vec;
}

void  qvector_wipe(qvector_t *vec, int v_size);
void  __qvector_grow(qvector_t *, int v_size, int extra);
void *__qvector_splice(qvector_t *, int v_size, int pos, int len, int dlen);
static inline void qvector_reset(qvector_t *vec, int v_size)
{
    vec->size += vec->skip;
    vec->tab  += vec->skip * v_size;
    vec->skip  = 0;
    vec->len   = 0;
}

static inline void *qvector_grow(qvector_t *vec, int v_size, int extra)
{
    if (vec->len + extra > vec->size)
        __qvector_grow(vec, v_size, extra);
    return vec->tab + vec->len * v_size;
}

static inline void *qvector_growlen(qvector_t *vec, int v_size, int extra)
{
    void *res;

    if (vec->len + extra > vec->size)
        __qvector_grow(vec, v_size, extra);
    res = vec->tab + vec->len * v_size;
    vec->len += extra;
    return res;
}

static inline void *
qvector_splice(qvector_t *vec, int v_size,
               int pos, int len, const void *tab, int dlen)
{
    char *res;

    assert (pos >= 0 && len >= 0 && dlen >= 0);
    assert ((unsigned)pos <= (unsigned)vec->len);
    assert ((unsigned)pos + (unsigned)len <= (unsigned)vec->len);

    if (__builtin_constant_p(dlen)) {
        if (dlen == 0 || (__builtin_constant_p(len) && len >= dlen)) {
            memmove(vec->tab + v_size * (pos + dlen),
                    vec->tab + v_size * (pos + len),
                    v_size * (vec->len - pos - len));
            vec->len += dlen - len;
            return vec->tab + v_size * pos;
        }
    }
    if (__builtin_constant_p(len) && len == 0 && pos == vec->len) {
        res = qvector_growlen(vec, v_size, dlen);
    } else {
        res = __qvector_splice(vec, v_size, pos, len, dlen);
    }
    return tab ? memcpy(res, tab, dlen * v_size) : res;
}

#define __QVECTOR_BASE(pfx, val_t) \
    typedef union pfx##_t {                                                 \
        qvector_t qv;                                                       \
        STRUCT_QVECTOR_T(val_t);                                            \
    } pfx##_t;                                                              \
                                                                            \
    static inline pfx##_t *                                                 \
    __##pfx##_init(pfx##_t *vec, void *buf, int blen, int bsize, int mp) {  \
        __qvector_init(&vec->qv, buf, blen, bsize, mp);                     \
        return vec;                                                         \
    }                                                                       \
    static inline void pfx##_wipe(pfx##_t *vec) {                           \
        qvector_wipe(&vec->qv, sizeof(val_t));                              \
    }                                                                       \
    static inline void pfx##_delete(pfx##_t **vec) {                        \
        if (likely(*vec)) {                                                 \
            qvector_wipe(&(*vec)->qv, sizeof(val_t));                       \
            p_delete(vec);                                                  \
        }                                                                   \
    }                                                                       \
                                                                            \
    static inline val_t *                                                   \
    __##pfx##_splice(pfx##_t *vec, int pos, int len, int dlen) {            \
        return __qvector_splice(&vec->qv, sizeof(val_t), pos, len, dlen);   \
    }                                                                       \
    static inline void pfx##_clip(pfx##_t *vec, int at) {                   \
        assert (0 <= at && at <= vec->len);                                 \
        __##pfx##_splice(vec, at, vec->len - at, 0);                        \
    }                                                                       \
    static inline void pfx##_shrink(pfx##_t *vec, int at) {                 \
        assert (0 <= at && at <= vec->len);                                 \
        vec->len -= at;                                                     \
    }                                                                       \
    static inline val_t *                                                   \
    pfx##_splice(pfx##_t *vec, int pos, int len,                            \
                 const val_t *tab, int dlen) {                              \
        return qvector_splice(&vec->qv, sizeof(val_t), pos, len, tab, dlen);\
    }                                                                       \
    static inline val_t *pfx##_grow(pfx##_t *vec, int extra) {              \
        return qvector_grow(&vec->qv, sizeof(val_t), extra);                \
    }                                                                       \
    static inline val_t *pfx##_growlen(pfx##_t *vec, int extra) {           \
        return qvector_growlen(&vec->qv, sizeof(val_t), extra);             \
    }

#define qvector_t(n, val_t)                 __QVECTOR_BASE(qv_##n, val_t)

#define qv_t(n)                             qv_##n##_t
#define __qv_sz(n)                          fieldsizeof(qv_t(n), tab[0])
#define __qv_init(n, vec, b, bl, bs, mp)    __qv_##n##_init(vec, b, bl, bs, mp)
#define qv_inita(n, vec, len) \
    ({ size_t __len = (len), _sz = __len * __qv_sz(n); \
       __qv_init(n, vec, alloca(_sz), 0, __len, MEM_STATIC); })
#define t_qv_init(n, vec, len) \
    ({ size_t __len = (len), _sz = __len * __qv_sz(n); \
       __qv_init(n, vec, t_new_raw(char, _sz), 0, __len, MEM_STACK); })

#define qv_init(n, vec)                     p_clear(vec, 1)
#define qv_clear(n, vec)                    qvector_reset(&(vec)->qv, __qv_sz(n))
#define qv_wipe(n, vec)                     qv_##n##_wipe(vec)
#define qv_deep_wipe(n, vec, wipe) \
    ({ qv_t(n) *__vec = (vec);              \
       qv_for_each_pos(n, __i, __vec) {     \
           wipe(&__vec->tab[__i]);          \
       }                                    \
       qv_wipe(n, __vec); })
#define qv_new(n)                           p_new(qv_t(n), 1)
#define qv_delete(n, vec)                   qv_##n##_delete(vec)

#define qv_last(n, vec)                     ({ qv_t(n) *__vec = (vec);  \
                                               assert (__vec->len > 0); \
                                               __vec->tab + __vec->len - 1; })

#define __qv_splice(n, vec, pos, l, dl)     __qv_##n##_splice(vec, pos, l, dl)
#define qv_splice(n, vec, pos, l, tab, dl)  qv_##n##_splice(vec, pos, l, tab, dl)
#define qv_grow(n, vec, extra)              qv_##n##_grow(vec, extra)
#define qv_growlen(n, vec, extra)           qv_##n##_growlen(vec, extra)
#define qv_clip(n, vec, len)                qv_##n##_clip(vec, len)
#define qv_shrink(n, vec, len)              qv_##n##_shrink(vec, len)
#define qv_skip(n, vec, len)                (void)__qv_splice(n, vec, 0, len, 0)
#define qv_remove(n, vec, i)                (void)__qv_splice(n, vec, i,   1, 0)
#define qv_pop(n, vec)                      (void)__qv_splice(n, vec, 0,   1, 0)

#define qv_insert(n, vec, i, v)             (*__qv_splice(n, vec, i, 0, 1) = (v))
#define qv_append(n, vec, v)                (*qv_growlen(n, vec, 1) = (v))
#define qv_push(n, vec, v)                  qv_insert(n, vec, 0, v)
#define qv_insertp(n, vec, i, v)            qv_insert(n, vec, i, *(v))
#define qv_appendp(n, vec, v)               qv_append(n, vec, *(v))
#define qv_pushp(n, vec, v)                 qv_push(n, vec, *(v))

#define qv_for_each_pos(n, pos, vec) \
    for (int pos = 0; pos < (vec)->len; pos++)

#define qv_for_each_pos_safe(n, pos, vec) \
    for (int pos = (vec)->len; pos-- > 0; )



/* Define several common types */
qvector_t(i16,    int16_t);
qvector_t(u16,    uint16_t);
qvector_t(i32,    int32_t);
qvector_t(u32,    uint32_t);
qvector_t(i64,    int64_t);
qvector_t(u64,    uint64_t);
qvector_t(void,   void *);
qvector_t(double, double);
qvector_t(str,    char *);
qvector_t(lstr,   lstr_t);
qvector_t(clstr,  clstr_t);
