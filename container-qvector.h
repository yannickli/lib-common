/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2015 INTERSEC SA                                   */
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
    }

typedef STRUCT_QVECTOR_T(uint8_t) qvector_t;

#ifdef __has_blocks
typedef int (BLOCK_CARET qvector_cmp_b)(const void *, const void *);
#endif

static inline qvector_t *
__qvector_init(qvector_t *vec, void *buf, int blen, int bsize, int mem_pool)
{
    *vec = (qvector_t){
        cast(uint8_t *, buf),
        blen,
        bsize,
        cast(flag_t, mem_pool),
    };
    return vec;
}

void  qvector_reset(qvector_t *vec, size_t v_size)
    __leaf;
void  qvector_wipe(qvector_t *vec, size_t v_size)
    __leaf;
void  __qvector_grow(qvector_t *, size_t v_size, size_t v_align, int extra)
    __leaf;
void  __qvector_optimize(qvector_t *, size_t v_size, size_t size, size_t v_align)
    __leaf;
void *__qvector_splice(qvector_t *, size_t v_size, size_t v_align,
                       int pos, int len, int dlen)
    __leaf;
#ifdef __has_blocks
void __qv_sort32(void *a, size_t n, qvector_cmp_b cmp);
void __qv_sort64(void *a, size_t n, qvector_cmp_b cmp);
void __qv_sort(void *a, size_t v_size, size_t n, qvector_cmp_b cmp);

static ALWAYS_INLINE void
__qvector_sort(qvector_t *vec, size_t v_size, qvector_cmp_b cmp)
{
    if (v_size == 8) {
        __qv_sort64(vec->tab, vec->len, cmp);
    } else
    if (v_size == 4) {
        __qv_sort32(vec->tab, vec->len, cmp);
    } else {
        __qv_sort(vec->tab, v_size, vec->len, cmp);
    }
}
void __qvector_diff(const qvector_t *vec1, const qvector_t *vec2,
                    qvector_t *add, qvector_t *del, qvector_t *inter,
                    size_t v_size, size_t v_align, qvector_cmp_b cmp);
int  __qvector_bisect(const qvector_t *vec, size_t v_size, const void *elt,
                      bool *found, qvector_cmp_b cmp);
int __qvector_find(const qvector_t *vec, size_t v_size, const void *elt,
                   bool sorted, qvector_cmp_b cmp);
bool __qvector_contains(const qvector_t *vec, size_t v_size, const void *elt,
                        bool sorted, qvector_cmp_b cmp);
void __qvector_uniq(qvector_t *vec, size_t v_size, qvector_cmp_b cmp);
#endif

/** \brief optimize vector for space.
 *
 * \param[in]   vec          the vector to optimize
 * \param[in]   v_size       sizeof(val_t) for this qvector
 * \param[in]   ratio        the ratio of garbage allowed.
 * \param[in]   extra_ratio  the extra size to add when resizing.
 *
 * If there is more than vec->len * (ratio / 100) empty cells, the array is
 * resize to vec->len + vec->len * (extra_ratio / 100).
 *
 * In particular, qvector_optimize(vec, ..., 0, 0) forces the vector
 * allocation to have no waste.
 */
static inline void
qvector_optimize(qvector_t *vec, size_t v_size, size_t v_align,
                 size_t ratio, size_t extra_ratio)
{
    size_t cur_waste = vec->size - vec->len;

    if (vec->len * ratio < 100 * cur_waste)
        __qvector_optimize(vec, v_size, v_align,
                           vec->len + vec->len * extra_ratio / 100);
}

static inline
void *qvector_grow(qvector_t *vec, size_t v_size, size_t v_align, int extra)
{
    ssize_t size = vec->len + extra;

    if (size > vec->size) {
        __qvector_grow(vec, v_size, v_align, extra);
    } else {
        ssize_t cursz = vec->size;

        if (unlikely(cursz * v_size > BUFSIZ && size * 8 < cursz))
            __qvector_optimize(vec, v_size, v_align, p_alloc_nr(size));
    }
    return vec->tab + vec->len * v_size;
}

static inline
void *qvector_growlen(qvector_t *vec, size_t v_size, size_t v_align, int extra)
{
    void *res;

    if (vec->len + extra > vec->size)
        __qvector_grow(vec, v_size, v_align, extra);
    res = vec->tab + vec->len * v_size;
    vec->len += extra;
    return res;
}

static inline void *
qvector_splice(qvector_t *vec, size_t v_size, size_t v_align,
               int pos, int len, const void *tab, int dlen)
{
    void *res;

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
        res = qvector_growlen(vec, v_size, v_align, dlen);
    } else {
        res = __qvector_splice(vec, v_size, v_align, pos, len, dlen);
    }
    return tab ? memcpy(res, tab, dlen * v_size) : res;
}

#define __QVECTOR_BASE_TYPE(pfx, cval_t, val_t)                             \
    typedef union pfx##_t {                                                 \
        qvector_t qv;                                                       \
        STRUCT_QVECTOR_T(val_t);                                            \
    } pfx##_t;

#ifdef __has_blocks
#define __QVECTOR_BASE_BLOCKS(pfx, cval_t, val_t) \
    typedef int (BLOCK_CARET pfx##_cmp_b)(cval_t *a, cval_t *b);            \
                                                                            \
    __unused__                                                              \
    static inline void pfx##_sort(pfx##_t *vec, pfx##_cmp_b cmp) {          \
        __qvector_sort(&vec->qv, sizeof(val_t), (qvector_cmp_b)cmp);        \
    }                                                                       \
    __unused__                                                              \
    static inline void                                                      \
    pfx##_diff(const pfx##_t *vec1, const pfx##_t *vec2,                    \
               pfx##_t *add, pfx##_t *del, pfx##_t *inter, pfx##_cmp_b cmp) \
    {                                                                       \
        __qvector_diff(&vec1->qv, &vec2->qv, add ? &add->qv : NULL,         \
                       del ? &del->qv : NULL, inter ? &inter->qv : NULL,    \
                       sizeof(val_t), alignof(val_t), (qvector_cmp_b)cmp);  \
    }                                                                       \
    __unused__                                                              \
    static inline                                                           \
    void pfx##_uniq(pfx##_t *vec, pfx##_cmp_b cmp) {                        \
        __qvector_uniq(&vec->qv, sizeof(val_t), (qvector_cmp_b)cmp);        \
    }                                                                       \
    __unused__                                                              \
    static inline                                                           \
    int pfx##_bisect(const pfx##_t *vec, cval_t v, bool *found,             \
                     pfx##_cmp_b cmp)                                       \
    {                                                                       \
        return __qvector_bisect(&vec->qv, sizeof(val_t), &v, found,         \
                                (qvector_cmp_b)cmp);                        \
    }                                                                       \
    __unused__                                                              \
    static inline                                                           \
    int pfx##_find(const pfx##_t *vec, cval_t v, bool sorted,               \
                   pfx##_cmp_b cmp)                                         \
    {                                                                       \
        return __qvector_find(&vec->qv, sizeof(val_t), &v, sorted,          \
                              (qvector_cmp_b)cmp);                          \
    }                                                                       \
    __unused__                                                              \
    static inline                                                           \
    bool pfx##_contains(const pfx##_t *vec, cval_t v, bool sorted,          \
                        pfx##_cmp_b cmp) {                                  \
        return __qvector_contains(&vec->qv, sizeof(val_t), &v, sorted,      \
                                  (qvector_cmp_b)cmp);                      \
    }
#else
#define __QVECTOR_BASE_BLOCKS(pfx, cval_t, val_t)
#endif

#define __QVECTOR_BASE_FUNCTIONS(pfx, cval_t, val_t) \
    __unused__                                                              \
    static inline pfx##_t *                                                 \
    __##pfx##_init(pfx##_t *vec, void *buf, int blen, int bsize, int mp) {  \
        __qvector_init(&vec->qv, buf, blen, bsize, mp);                     \
        return vec;                                                         \
    }                                                                       \
    __unused__                                                              \
    static inline void pfx##_wipe(pfx##_t *vec) {                           \
        qvector_wipe(&vec->qv, sizeof(val_t));                              \
    }                                                                       \
    __unused__                                                              \
    static inline void pfx##_delete(pfx##_t **vec) {                        \
        if (likely(*vec)) {                                                 \
            qvector_wipe(&(*vec)->qv, sizeof(val_t));                       \
            p_delete(vec);                                                  \
        }                                                                   \
    }                                                                       \
                                                                            \
    __unused__                                                              \
    static inline val_t *                                                   \
    __##pfx##_splice(pfx##_t *vec, int pos, int len, int dlen) {            \
        return (val_t *)__qvector_splice(&vec->qv, sizeof(val_t),           \
                                         alignof(val_t), pos, len, dlen);   \
    }                                                                       \
    __unused__                                                              \
    static inline void pfx##_clip(pfx##_t *vec, int at) {                   \
        assert (0 <= at && at <= vec->len);                                 \
        __##pfx##_splice(vec, at, vec->len - at, 0);                        \
    }                                                                       \
    __unused__                                                              \
    static inline void pfx##_shrink(pfx##_t *vec, int at) {                 \
        assert (0 <= at && at <= vec->len);                                 \
        vec->len -= at;                                                     \
    }                                                                       \
    __unused__                                                              \
    static inline val_t *                                                   \
    pfx##_splice(pfx##_t *vec, int pos, int len, cval_t *tab, int dlen) {   \
        void *res = qvector_splice(&vec->qv, sizeof(val_t), alignof(val_t), \
                                   pos, len, tab, dlen);                    \
        return cast(val_t *, res);                                          \
    }                                                                       \
    __unused__                                                              \
    static inline void pfx##_optimize(pfx##_t *vec, size_t r1, size_t r2) { \
        qvector_optimize(&vec->qv, sizeof(val_t), alignof(val_t), r1, r2);  \
    }                                                                       \
    __unused__                                                              \
    static inline val_t *pfx##_grow(pfx##_t *vec, int extra) {              \
        void *res = qvector_grow(&vec->qv, sizeof(val_t), alignof(val_t),   \
                                 extra);                                    \
        return cast(val_t *, res);                                          \
    }                                                                       \
    __unused__                                                              \
    static inline val_t *pfx##_growlen(pfx##_t *vec, int extra) {           \
        void *res = qvector_growlen(&vec->qv, sizeof(val_t), alignof(val_t),\
                                    extra);                                 \
        return cast(val_t *, res);                                          \
    }                                                                       \
    __unused__                                                              \
    static inline void pfx##_qsort(pfx##_t *vec,                            \
                                   int (*cb)(cval_t *, cval_t *)) {         \
        qsort(vec->qv.tab, vec->qv.len, sizeof(cval_t),                     \
                (int (*)(const void *, const void *))cb);                   \
    }                                                                       \
    __unused__                                                              \
    static inline pfx##_t *pfx##_copy(pfx##_t *vec_out,                     \
                                      const pfx##_t *vec_in) {              \
        vec_out->len = 0;                                                   \
        p_copy(pfx##_growlen(vec_out, vec_in->len),                         \
               vec_in->tab, vec_in->len);                                   \
        return vec_out;                                                     \
    }                                                                       \
    __QVECTOR_BASE_BLOCKS(pfx, cval_t, val_t)

/** Declare a new vector type.
 *
 * qvector_type_t and qvector_funcs_t allow recursive declaraion of structure
 * types that contain a qvector of itself:
 *
 * \code
 * qvector_type_t(vec_type, struct mytype_t);
 * struct mytype_t {
 *     qv_t(vec_type) children;
 * };
 * qvector_funcs_t(vec_type, struct mytype_t);
 * \endcode
 *
 * For most use cases you should use qvector_t() that declares both the type
 * and the functions.
 */
#define qvector_type_t(n, val_t) \
    __QVECTOR_BASE_TYPE(qv_##n, val_t const, val_t)
#define qvector_funcs_t(n, val_t) \
    __QVECTOR_BASE_FUNCTIONS(qv_##n, val_t const, val_t)

#define qvector_t(n, val_t)                                                  \
    qvector_type_t(n, val_t);                                                \
    qvector_funcs_t(n, val_t);

#define qv_t(n)                             qv_##n##_t
#define __qv_sz(n)                          fieldsizeof(qv_t(n), tab[0])
#define __qv_init(n, vec, b, bl, bs, mp)    __qv_##n##_init(vec, b, bl, bs, mp)
#define qv_init_static(n, vec, tab, len) \
    ({ size_t __len = (len); \
       __qv_init(n, vec, tab, __len, __len, MEM_STATIC); })
#define qv_inita(n, vec, len) \
    ({ size_t __len = (len), _sz = __len * __qv_sz(n); \
       __qv_init(n, vec, alloca(_sz), 0, __len, MEM_STATIC); })
#define t_qv_init(n, vec, len) \
    ({ size_t __len = (len); \
       __qv_init(n, vec, t_new_raw(fieldtypeof(qv_t(n), tab[0]), __len), \
                 0, __len, MEM_STACK); })

#define qv_init(n, vec)  \
    ({ qv_t(n) *__vec = (vec);              \
       p_clear(__vec, 1);                   \
    })

#define qv_clear(n, vec)  qvector_reset(&(vec)->qv, __qv_sz(n))
#define qv_deep_clear(n, vec, wipe) \
    ({ qv_t(n) *__vec = (vec);               \
       qv_for_each_pos_safe(n, __i, __vec) { \
           wipe(&__vec->tab[__i]);           \
       }                                     \
       qv_clear(n, __vec); })

#define qv_wipe(n, vec)  qv_##n##_wipe(vec)
#define qv_deep_wipe(n, vec, wipe) \
    ({ qv_t(n) *__vec = (vec);               \
       qv_for_each_pos_safe(n, __i, __vec) { \
           wipe(&__vec->tab[__i]);           \
       }                                     \
       qv_wipe(n, __vec); })

#define qv_delete(n, vec)  qv_##n##_delete(vec)
#define qv_deep_delete(n, vecp, wipe) \
    ({ qv_t(n) **__vecp = (vecp);                  \
       if (likely(*__vecp)) {                      \
           qv_for_each_pos_safe(n, __i, *__vecp) { \
               wipe(&(*__vecp)->tab[__i]);         \
           }                                       \
           qv_delete(n, __vecp);                   \
       } })

#define qv_new(n)  p_new(qv_t(n), 1)

#ifdef __has_blocks
/* You must be in a .blk to use qv_sort, because it expects blocks ! */
#define qv_sort(n)                          qv_##n##_sort
#define qv_cmp_b(n)                         qv_##n##_cmp_b
#endif
#define qv_qsort(n, vec, cmp)               qv_##n##_qsort(vec, cmp)

#define qv_last(n, vec)                     ({ const qv_t(n) *__vec = (vec); \
                                               assert (__vec->len > 0);      \
                                               __vec->tab + __vec->len - 1; })

#define __qv_splice(n, vec, pos, l, dl)     __qv_##n##_splice(vec, pos, l, dl)
#define qv_splice(n, vec, pos, l, tab, dl)  qv_##n##_splice(vec, pos, l, tab, dl)
#define qv_optimize(n, vec, r1, r2)         qv_##n##_optimize(vec, r1, r2)

#define qv_grow(n, vec, extra)              qv_##n##_grow(vec, extra)
#define qv_growlen(n, vec, extra)           qv_##n##_growlen(vec, extra)

/** \brief keep only the first len elements */
#define qv_clip(n, vec, len)                qv_##n##_clip(vec, len)
/** \brief shrink the vector length by len */
#define qv_shrink(n, vec, len)              qv_##n##_shrink(vec, len)
/** \brief skip the first len elements */
#define qv_skip(n, vec, len)                (void)__qv_splice(n, vec, 0, len, 0)

#define qv_remove(n, vec, i)                (void)__qv_splice(n, vec, i,   1, 0)
#define qv_pop(n, vec)                      (void)__qv_splice(n, vec, 0,   1, 0)

#define qv_insert(n, vec, i, v)                                     \
    ({                                                              \
        typeof(*(vec)->tab) __v = (v);                              \
        *__qv_splice(n, vec, i, 0, 1) = __v;                        \
    })
#define qv_append(n, vec, v)                                        \
    ({                                                              \
        typeof(*(vec)->tab) __v = (v);                              \
        *qv_growlen(n, vec, 1) = (__v);                             \
    })
#define qv_push(n, vec, v)                  qv_insert(n, vec, 0, (v))
#define qv_insertp(n, vec, i, v)            qv_insert(n, vec, i, *(v))
#define qv_appendp(n, vec, v)               qv_append(n, vec, *(v))
#define qv_pushp(n, vec, v)                 qv_push(n, vec, *(v))

#define qv_copy(n, vec_out, vec_in)         qv_##n##_copy(vec_out, vec_in)

#define qv_for_each_pos(n, pos, vec)                                         \
    ASSERT_COMPATIBLE((vec)->tab[0], ((const qv_t(n) *)NULL)->tab[0]);       \
    tab_for_each_pos(pos, vec)

#define qv_for_each_ptr(n, ptr, vec)                                         \
    ASSERT_COMPATIBLE((vec)->tab[0], ((const qv_t(n) *)NULL)->tab[0]);       \
    tab_for_each_ptr(ptr, vec)

#define qv_for_each_entry(n, e, vec)                                         \
    ASSERT_COMPATIBLE((vec)->tab[0], ((const qv_t(n) *)NULL)->tab[0]);       \
    tab_for_each_entry(e, vec)

#define qv_for_each_pos_rev(n, pos, vec)                                     \
    ASSERT_COMPATIBLE((vec)->tab[0], ((const qv_t(n) *)NULL)->tab[0]);       \
    tab_for_each_pos_rev(pos, vec)

#define qv_for_each_pos_safe(n, pos, vec)                                    \
    qv_for_each_pos_rev(n, pos, vec)

#ifdef __has_blocks
/** \brief build the difference and intersection vectors by comparing elements
 *         of vec1 and vec2
 *
 * This generates a qv_diff function which can be used like that, for example:
 *
 *   qv_diff(u32)(&vec1, &vec2, &add, &del, &inter,
 *                ^int (const uint32_t *v1, const uint32_t *v2) {
 *       return CMP(*v1, *v2);
 *   });
 *
 * \param[in]   vec1  the vector to filter (not modified)
 * \param[in]   vec2  the vector containing the elements to filter
 *                    from vec1 (not modified)
 * \param[out]  add   the vector of v2 values not in v1 (may be NULL if not
 *                    interested in added values)
 * \param[out]  del   the vector of v1 values not in v2 (may be NULL if not
 *                    interested in deleted values)
 * \param[out]  inter the vector of v1 values also in v2 (may be NULL if not
 *                    interested in intersection values)
 * \param[in]   cmp   comparison function for the elements of the vectors
 *
 * You must be in a .blk to use qv_diff, because it expects blocks.
 *
 * WARNING: vec1 and vec2 must be sorted and uniq'ed.
 */
#define qv_diff(n)                          qv_##n##_diff

/** Remove duplicated entries from a vector.
 *
 * This takes a sorted vector as input and remove duplicated entries.
 *
 * \param[in,out]   vec the vector to filter
 * \param[in]       cmp comparison callback for the elements of the vector.
 */
#define qv_uniq(n)                          qv_##n##_uniq

/** Lookup the position of the entry in a sorted vector.
 *
 * This takes an entry and a sorted vector and lookup the entry within the
 * vector using a binary search.
 *
 * \param[in]       vec the vector
 * \param[in]       v   the value to lookup
 * \param[out]      found a pointer to a boolean that is set to true if the
 *                  value is found in the vector, false if not. Can be NULL.
 * \param[in]       cmp comparison callback for the elements of the vector.
 * \return          the position of \p v if found, the position where to
 *                  insert \p v if not already present in the vector.
 */
#define qv_bisect(n)                        qv_##n##_bisect

/** Find the position of the entry in a vector.
 *
 * This takes an entry and a vector. If the vector is sorted, a binary search
 * is performed, otherwise a linear scan is performed.
 *
 * \param[in]       vec    the vector
 * \param[in]       v      the value to lookup
 * \param[in]       sorted if true the vector is considered as being sorted
 *                         with the ordering of the provided comparator.
 * \param[in]       cmp    comparison callback for the elements of the vector
 * \return          index of the element if found in the vector, -1 otherwise.
 */
#define qv_find(n)                      qv_##n##_find

/** Check if a vector contains a specific entry.
 *
 * This takes an entry and a vector. If the vector is sorted, a binary search
 * is performed, otherwise a linear scan is performed.
 *
 * \param[in]       vec    the vector
 * \param[in]       v      the value to lookup
 * \param[in]       sorted if true the vector is considered as being sorted
 *                         with the ordering of the provided comparator.
 * \param[in]       cmp    comparison callback for the elements of the vector
 * \return          true if the element was found in the vector, false
 *                  otherwise.
 */
#define qv_contains(n)                      qv_##n##_contains
#endif


/* Define several common types */
qvector_t(i8,     int8_t);
qvector_t(u8,     uint8_t);
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
qvector_t(pstream, pstream_t);

qvector_t(cvoid, const void *);
qvector_t(cstr,  const char *);
qvector_t(sbp,   sb_t *);
