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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_STR_L_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_STR_L_H

/** \brief representation of a string with length.
 *
 * This type provides a unified way to talk about immutable strings (unlike
 * #sb_t that are mutable buffers).
 *
 * It remembers wether the string has been allocated through p_new, t_new, a
 * pool or is static.
 */
typedef struct lstr_t {
    union {
        const char *s;
        char       *v;
        void       *data;
    };
    int     len;
    flag_t  mem_pool : 3;
} lstr_t;

/* Static initializers {{{ */

#define LSTR_INIT(s_, len_)     { { (s_) }, (len_), 0 }
#define LSTR_INIT_V(s, len)     (lstr_t)LSTR_INIT(s, len)
#define LSTR_IMMED(str)         LSTR_INIT(""str, sizeof(str) - 1)
#define LSTR_IMMED_V(str)       LSTR_INIT_V(""str, sizeof(str) - 1)
#define LSTR(str)               ({ const char *__s = (str); \
                                   LSTR_INIT_V(__s, (int)strlen(__s)); })
#define LSTR_NULL               LSTR_INIT(NULL, 0)
#define LSTR_NULL_V             LSTR_INIT_V(NULL, 0)
#define LSTR_EMPTY              LSTR_INIT("", 0)
#define LSTR_EMPTY_V            LSTR_INIT_V("", 0)
#define LSTR_SB(sb)             LSTR_INIT((sb)->data, (sb)->len)
#define LSTR_SB_V(sb)           LSTR_INIT_V((sb)->data, (sb)->len)
#define LSTR_PS(ps)             LSTR_INIT((ps)->s, ps_len(ps))
#define LSTR_PS_V(ps)           LSTR_INIT_V((ps)->s, ps_len(ps))
#define LSTR_PTR(start, end)    LSTR_INIT((start), (end) - (start))
#define LSTR_PTR_V(start, end)  LSTR_INIT_V((start), (end) - (start))

#define LSTR_DATA(data, len)    LSTR_INIT((const char *)(data), (len))
#define LSTR_DATA_V(data, len)  (lstr_t)LSTR_DATA(data, len)

#define LSTR_CARRAY(carray)     LSTR_DATA((carray), sizeof(carray))
#define LSTR_CARRAY_V(carray)   (lstr_t)LSTR_CARRAY(carray)

#define LSTR_FMT_ARG(s_)      (s_).len, (s_).s

#define LSTR_OPT(str)         ({ const char *__s = (str);              \
                                 __s ? LSTR_INIT_V(__s, strlen(__s))   \
                                     : LSTR_NULL_V; })

/* obsolete stuff, please try not to use anymore */
#define LSTR_STR_V      LSTR
#define LSTR_OPT_STR_V  LSTR_OPT

/* }}} */
/* Base helpers {{{ */

/** \brief lstr_dup_* helper.
 */
static ALWAYS_INLINE lstr_t lstr_init_(const void *s, int len, unsigned flags)
{
    return (lstr_t){ { (const char *)s }, len, flags };
}

static ALWAYS_INLINE
lstr_t mp_lstr_init(mem_pool_t *mp, const void *s, int len)
{
    mp = mp ?: &mem_pool_libc;
    return lstr_init_(s, len, mp->mem_pool & MEM_POOL_MASK);
}

/** Initialize a lstr_t from the content of a file.
 *
 * The function takes the prot and the flags to be passed to the mmap call.
 */
int lstr_init_from_file(lstr_t *dst, const char *path, int prot, int flags);

/** Initialize a lstr_t from the content of a file pointed by a fd.
 */
int lstr_init_from_fd(lstr_t *dst, int fd, int prot, int flags);

/** lstr_wipe helper.
 */
void lstr_munmap(lstr_t *dst);
#define lstr_munmap(...)  lstr_munmap_DO_NOT_CALL_DIRECTLY(__VA_ARGS__)


/** \brief lstr_copy_* helper.
 */
static ALWAYS_INLINE
void mp_lstr_copy_(mem_pool_t *mp, lstr_t *dst, const void *s, int len)
{
    mp = mp ?: &mem_pool_libc;
    if (dst->mem_pool == (mp->mem_pool & MEM_POOL_MASK)) {
        mp_delete(mp, &dst->v);
    } else
    if (dst->mem_pool == MEM_MMAP) {
        (lstr_munmap)(dst);
    } else {
        ifree(dst->v, dst->mem_pool);
    }
    if (s == NULL) {
        *dst = lstr_init_(NULL, 0, MEM_STATIC);
    } else {
        *dst = lstr_init_(s, len, mp->mem_pool & MEM_POOL_MASK);
    }
}

/** \brief sets \v dst to a new \v mp allocated lstr from its arguments.
 */
static inline
void mp_lstr_copys(mem_pool_t *mp, lstr_t *dst, const char *s, int len)
{
    if (s) {
        if (len < 0) {
            len = strlen(s);
        }
        mp = mp ?: &mem_pool_libc;
        mp_lstr_copy_(mp, dst, mp_dupz(mp, s, len), len);
    } else {
        mp_lstr_copy_(&mem_pool_static, dst, NULL, 0);
    }
}

/** \brief sets \v dst to a new \v mp allocated lstr from its arguments.
 */
static inline void mp_lstr_copy(mem_pool_t *mp, lstr_t *dst, const lstr_t src)
{
    if (src.s) {
        mp = mp ?: &mem_pool_libc;
        mp_lstr_copy_(mp, dst, mp_dupz(mp, src.s, src.len), src.len);
    } else {
        mp_lstr_copy_(&mem_pool_static, dst, NULL, 0);
    }
}

/** \brief returns new \v mp allocated lstr from its arguments.
 */
static inline lstr_t mp_lstr_dups(mem_pool_t *mp, const char *s, int len)
{
    if (!s) {
        return LSTR_NULL_V;
    }
    if (len < 0) {
        len = strlen(s);
    }
    return mp_lstr_init(mp, mp_dupz(mp, s, len), len);
}

/** \brief returns new \v mp allocated lstr from its arguments.
 */
static inline lstr_t mp_lstr_dup(mem_pool_t *mp, const lstr_t s)
{
    if (!s.s)
        return LSTR_NULL_V;
    return mp_lstr_init(mp, mp_dupz(mp, s.s, s.len), s.len);
}

/** \brief ensure \p s is \p mp or heap allocated.
 */
static inline void mp_lstr_persists(mem_pool_t *mp, lstr_t *s)
{
    mp = mp ?: &mem_pool_libc;
    if (s->mem_pool != MEM_LIBC
    &&  s->mem_pool != (mp->mem_pool & MEM_POOL_MASK))
    {
        s->s        = (char *)mp_dupz(mp, s->s, s->len);
        s->mem_pool = mp->mem_pool & MEM_POOL_MASK;
    }
}

/** \brief duplicates \p v on the t_stack and reverse its content.
 *
 * This function is not unicode-aware.
 */
static inline lstr_t mp_lstr_dup_ascii_reversed(mem_pool_t *mp, const lstr_t v)
{
    char *str;

    if (!v.s) {
        return v;
    }

    str = mp_new_raw(mp, char, v.len + 1);

    for (int i = 0; i < v.len; i++) {
        str[i] = v.s[v.len - i - 1];
    }
    str[v.len] = '\0';

    return mp_lstr_init(mp, str, v.len);
}

/** \brief duplicates \p v on the mem_pool and reverse its content.
 *
 * This function reverse character by character, which means that the result
 * contains the same characters in the reversed order but each character is
 * preserved in it's origin byte-wise order. This guarantees that both the
 * source and the destination are valid utf8 strings.
 *
 * In case of error, LSTR_NULL_V is returned.
 */
static inline lstr_t mp_lstr_dup_utf8_reversed(mem_pool_t *mp, const lstr_t v)
{
    int prev_off = 0;
    char *str;

    if (!v.s) {
        return v;
    }

    str = mp_new_raw(mp, char, v.len + 1);
    while (prev_off < v.len) {
        int off = prev_off;
        int c = utf8_ngetc_at(v.s, v.len, &off);

        if (unlikely(c < 0)) {
            return LSTR_NULL_V;
        }
        memcpy(str + v.len - off, v.s + prev_off, off - prev_off);
        prev_off = off;
    }
    return mp_lstr_init(mp, str, v.len);
}

/** \brief concatenates its argument to form a new lstr on the mem pool.
 */
static inline
lstr_t mp_lstr_cat(mem_pool_t *mp, const lstr_t s1, const lstr_t s2)
{
    int    len;
    lstr_t res;
    void  *s;

    if (unlikely(!s1.s && !s2.s)) {
        return LSTR_NULL_V;
    }

    len = s1.len + s2.len;
    res = mp_lstr_init(mp, mp_new_raw(mp, char, len + 1), len);
    s = (void *)res.v;
    s = mempcpy(s, s1.s, s1.len);
    mempcpyz(s, s2.s, s2.len);
    return res;
}

/** \brief concatenates its argument to form a new lstr on the mem pool.
 */
static inline
lstr_t mp_lstr_cat3(mem_pool_t *mp, const lstr_t s1, const lstr_t s2,
                    const lstr_t s3)
{
    int    len;
    lstr_t res;
    void  *s;

    if (unlikely(!s1.s && !s2.s && !s3.s)) {
        return LSTR_NULL_V;
    }

    len = s1.len + s2.len + s3.len;
    res = mp_lstr_init(mp, mp_new_raw(mp, char, len + 1), len);
    s = (void *)res.v;
    s = mempcpy(s, s1.s, s1.len);
    s = mempcpy(s, s2.s, s2.len);
    mempcpyz(s, s3.s, s3.len);
    return res;
}

/** \brief wipe a lstr_t (frees memory if needed).
 * This flavour assumes that the passed memory pool is the one to deallocate
 * from if the lstr_t is known as beeing allocated in a pool.
 */
static inline void mp_lstr_wipe(mem_pool_t *mp, lstr_t *s)
{
    mp_lstr_copy_(mp, s, NULL, 0);
}

/* }}} */
/* Transfer & static pool {{{ */

/** \brief copies \v src into \dst tranferring memory ownership to \v dst.
 */
static inline void lstr_transfer(lstr_t *dst, lstr_t *src)
{
    mp_lstr_copy_(ipool(src->mem_pool), dst, src->s, src->len);
    src->mem_pool = MEM_STATIC;
}

struct sb_t;

/** \brief copies \p src into \p dst transferring memory ownershipt to \p dst
 *
 * The \p src is a string buffer that will get reinitilized by the operation
 * as it loses ownership to the buffer.
 *
 * If \p keep_pool is false, the function ensures the memory will be allocated
 * on the heap (\ref sb_detach). If \p keep_pool is true, the memory will be
 * transfered as-is including the allocation pool.
 */
void lstr_transfer_sb(lstr_t *dst, struct sb_t *sb, bool keep_pool);

/** \brief copies a constant of \v s into \v dst.
 */
static inline void lstr_copyc(lstr_t *dst, const lstr_t s)
{
    mp_lstr_copy_(&mem_pool_static, dst, s.s, s.len);
}

/** \brief returns a constant copy of \v s.
 */
static inline lstr_t lstr_dupc(const lstr_t s)
{
    return lstr_init_(s.s, s.len, MEM_STATIC);
}

/* }}} */
/* Heap allocations {{{ */

/** \brief wipe a lstr_t (frees memory if needed).
 */
static inline void lstr_wipe(lstr_t *s)
{
    return mp_lstr_wipe(NULL, s);
}

/** \brief returns new libc allocated lstr from its arguments.
 */
static inline lstr_t lstr_dups(const char *s, int len)
{
    return mp_lstr_dups(NULL, s, len);
}

/** \brief returns new libc allocated lstr from its arguments.
 */
static inline lstr_t lstr_dup(const lstr_t s)
{
    return mp_lstr_dup(NULL, s);
}

/** \brief sets \v dst to a new libc allocated lstr from its arguments.
 */
static inline void lstr_copys(lstr_t *dst, const char *s, int len)
{
    return mp_lstr_copys(NULL, dst, s, len);
}

/** \brief sets \v dst to a new libc allocated lstr from its arguments.
 */
static inline void lstr_copy(lstr_t *dst, const lstr_t src)
{
    return mp_lstr_copy(NULL, dst, src);
}

/** \brief force lstr to be heap-allocated.
 *
 * This function ensure the lstr_t is allocated on the heap and thus is
 * guaranteed to be persistent.
 */
static inline void lstr_persists(lstr_t *s)
{
    return mp_lstr_persists(NULL, s);
}

/** \brief duplicates \p v on the heap and reverse its content.
 *
 * This function is not unicode-aware.
 */
static inline lstr_t lstr_dup_ascii_reversed(const lstr_t v)
{
    return mp_lstr_dup_ascii_reversed(NULL, v);
}

/** \brief duplicates \p v on the heap and reverse its content.
 *
 * This function reverse character by character, which means that the result
 * contains the same characters in the reversed order but each character is
 * preserved in it's origin byte-wise order. This guarantees that both the
 * source and the destination are valid utf8 strings.
 *
 * In case of error, LSTR_NULL_V is returned.
 */
static inline lstr_t lstr_dup_utf8_reversed(const lstr_t v)
{
    return mp_lstr_dup_utf8_reversed(NULL, v);
}

/** \brief concatenates its argument to form a new lstr on the heap.
 */
static inline lstr_t lstr_cat(const lstr_t s1, const lstr_t s2)
{
    return mp_lstr_cat(NULL, s1, s2);
}

/** \brief concatenates its argument to form a new lstr on the heap.
 */
static inline lstr_t lstr_cat3(const lstr_t s1, const lstr_t s2,
                               const lstr_t s3)
{
    return mp_lstr_cat3(NULL, s1, s2, s3);
}

/* }}} */
/* t_stack allocation {{{ */

/** \brief returns a duplicated lstr from the mem stack.
 */
static inline lstr_t t_lstr_dup(const lstr_t s)
{
    return mp_lstr_dup(t_pool(), s);
}

/** \brief returns a duplicated lstr from the mem stack.
 */
static inline lstr_t t_lstr_dups(const char *s, int len)
{
    return mp_lstr_dups(t_pool(), s, len);
}

/** \brief sets \v dst to a mem stack allocated copy of its arguments.
 */
static inline void t_lstr_copys(lstr_t *dst, const char *s, int len)
{
    return mp_lstr_copys(t_pool(), dst, s, len);
}

/** \brief sets \v dst to a mem stack allocated copy of its arguments.
 */
static inline void t_lstr_copy(lstr_t *dst, const lstr_t s)
{
    return mp_lstr_copy(t_pool(), dst, s);
}

/** \brief force lstr to be t_stack-allocated.
 *
 * This function ensure the lstr_t is allocated on the t_stack and thus is
 * guaranteed to be persistent.
 */
static inline void t_lstr_persists(lstr_t *s)
{
    return mp_lstr_persists(t_pool(), s);
}

/** \brief duplicates \p v on the t_stack and reverse its content.
 *
 * This function is not unicode-aware.
 */
static inline lstr_t t_lstr_dup_ascii_reversed(const lstr_t v)
{
    return mp_lstr_dup_ascii_reversed(t_pool(), v);
}

/** \brief duplicates \p v on the t_stack and reverse its content.
 *
 * This function reverse character by character, which means that the result
 * contains the same characters in the reversed order but each character is
 * preserved in it's origin byte-wise order. This guarantees that both the
 * source and the destination are valid utf8 strings.
 *
 * In case of error, LSTR_NULL_V is returned.
 */
static inline lstr_t t_lstr_dup_utf8_reversed(const lstr_t v)
{
    return mp_lstr_dup_utf8_reversed(t_pool(), v);
}

/** \brief concatenates its argument to form a new lstr on the t_stack.
 */
static inline lstr_t t_lstr_cat(const lstr_t s1, const lstr_t s2)
{
    return mp_lstr_cat(t_pool(), s1, s2);
}

/** \brief concatenates its argument to form a new lstr on the t_stack.
 */
static inline lstr_t t_lstr_cat3(const lstr_t s1, const lstr_t s2,
                                 const lstr_t s3)
{
    return mp_lstr_cat3(t_pool(), s1, s2, s3);
}

/** \brief return the left-trimmed lstr.
 */
static __must_check__ inline lstr_t lstr_ltrim(lstr_t s)
{
    while (s.len && isspace((unsigned char)s.s[0])) {
        s.s++;
        s.len--;
    }

    s.mem_pool = MEM_STATIC;
    return s;
}

/** \brief return the right-trimmed lstr.
 */
static __must_check__ inline lstr_t lstr_rtrim(lstr_t s)
{
    while (s.len && isspace((unsigned char)s.s[s.len - 1])) {
        s.len--;
    }

    return s;
}

/** \brief return the trimmed lstr.
 */
static __must_check__ inline lstr_t lstr_trim(lstr_t s)
{
    return lstr_rtrim(lstr_ltrim(s));
}

/* }}} */
/* r_pool allocation {{{ */

/** \brief returns a duplicated lstr from the mem stack.
 */
static inline lstr_t r_lstr_dup(const lstr_t s)
{
    return mp_lstr_dup(r_pool(), s);
}

/** \brief concatenates its argument to form a new lstr on the r_stack.
 */
static inline lstr_t r_lstr_cat(const lstr_t s1, const lstr_t s2)
{
    return mp_lstr_cat(r_pool(), s1, s2);
}

/* }}} */
/* Comparisons {{{ */

/** \brief returns "memcmp" ordering of \v s1 and \v s2.
 */
static ALWAYS_INLINE int lstr_cmp(const lstr_t *s1, const lstr_t *s2)
{
    /* workaround for a warning of -Wstringop-overflow in gcc 8
     * see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89699
     */
    size_t len = MIN((size_t)s1->len, (size_t)s2->len);

    return memcmp(s1->s, s2->s, len) ?: CMP(s1->len, s2->len);
}

/** \brief returns "memcmp" ordering of lowercase \v s1 and lowercase \v s2.
 */
static inline int lstr_ascii_icmp(const lstr_t *s1, const lstr_t *s2)
{
    int min = MIN(s1->len, s2->len);

    for (int i = 0; i < min; i++) {
        int a = tolower((unsigned char)s1->s[i]);
        int b = tolower((unsigned char)s2->s[i]);

        if (a != b) {
            return CMP(a, b);
        }
    }

    return CMP(s1->len, s2->len);
}

/** \brief returns whether \v s1 and \v s2 contents are equal.
 */
static ALWAYS_INLINE bool lstr_equal(const lstr_t *s1, const lstr_t *s2)
{
    return s1->len == s2->len && memcmp(s1->s, s2->s, s1->len) == 0;
}

/** \brief returns whether \v s1 and \v s2 contents are equal.
 */
static ALWAYS_INLINE bool lstr_equal2(const lstr_t s1, const lstr_t s2)
{
    return lstr_equal(&s1, &s2);
}

/** \brief returns whether \p s1 and \p s2 contents are case-insentively equal.
 *
 * This function should only be used in case you have a small number of
 * comparison to perform. If you need to perform a lot of checks with the
 * exact same string, you should first lowercase (or uppercase) both string
 * and use the case-sensitive equality that will be much more efficient.
 *
 * This function is not unicode-aware.
 */
static inline bool lstr_ascii_iequal(const lstr_t s1, const lstr_t s2)
{
    if (s1.len != s2.len) {
        return false;
    }
    for (int i = 0; i < s1.len; i++) {
        if (tolower((unsigned char)s1.s[i]) != tolower((unsigned char)s2.s[i]))
        {
            return false;
        }
    }
    return true;
}

/** \brief returns whether \v s1 contains substring \v s2.
 */
static ALWAYS_INLINE bool lstr_contains(const lstr_t s1, const lstr_t s2)
{
    return memmem(s1.data, s1.len, s2.data, s2.len) != NULL;
}

/** \brief returns whether \v s starts with \v p
 */
static ALWAYS_INLINE bool lstr_startswith(const lstr_t s, const lstr_t p)
{
    return s.len >= p.len && memcmp(s.s, p.s, p.len) == 0;
}

/** \brief returns whether \c s starts with \c c.
 */
static ALWAYS_INLINE bool lstr_startswithc(const lstr_t s, int c)
{
    return s.len >= 1 && s.s[0] == c;
}

/** \brief returns whether \c s ends with \c p.
 */
static ALWAYS_INLINE bool lstr_endswith(const lstr_t s, const lstr_t p)
{
    return s.len >= p.len && memcmp(s.s + s.len - p.len, p.s, p.len) == 0;
}

/** \brief returns whether \c s ends with \c c.
 */
static ALWAYS_INLINE bool lstr_endswithc(const lstr_t s, int c)
{
    return s.len >= 1 && s.s[s.len - 1] == c;
}

/** \brief returns whether \p s starts with \p p case-insenstively.
 *
 * \sa lstr_iequal, lstr_startswith
 *
 * This function is not unicode-aware.
 */
static inline bool lstr_ascii_istartswith(const lstr_t s, const lstr_t p)
{
    if (s.len < p.len) {
        return false;
    }
    return lstr_ascii_iequal(LSTR_INIT_V(s.s, p.len), p);
}

/** \brief returns whether \p s ends with \p p case-insenstively.
 *
 * \sa lstr_iequal, lstr_endswith
 * This function is not unicode-aware.
 */
static inline bool lstr_ascii_iendswith(const lstr_t s, const lstr_t p)
{
    if (s.len < p.len) {
        return false;
    }
    return lstr_ascii_iequal(LSTR_INIT_V(s.s + s.len - p.len, p.len), p);
}

/** \brief performs utf8-aware, case-insensitive comparison.
 */
static ALWAYS_INLINE int lstr_utf8_icmp(const lstr_t *s1, const lstr_t *s2)
{
    return utf8_stricmp(s1->s, s1->len, s2->s, s2->len, false);
}

/** \brief performs utf8-aware, case-sensitive comparison.
 */
static ALWAYS_INLINE int lstr_utf8_cmp(const lstr_t *s1, const lstr_t *s2)
{
    return utf8_strcmp(s1->s, s1->len, s2->s, s2->len, false);
}

/** \brief performs utf8-aware, case-insensitive equality check.
 */
static ALWAYS_INLINE bool lstr_utf8_iequal(const lstr_t *s1, const lstr_t *s2)
{
    return utf8_striequal(s1->s, s1->len, s2->s, s2->len, false);
}

/** \brief performs utf8-aware, case-insensitive equality check.
 */
static ALWAYS_INLINE bool lstr_utf8_iequal2(const lstr_t s1, const lstr_t s2)
{
    return lstr_utf8_iequal(&s1, &s2);
}

/** \brief performs utf8-aware, case-sensitive equality check.
 */
static ALWAYS_INLINE bool lstr_utf8_equal(const lstr_t *s1, const lstr_t *s2)
{
    return utf8_strequal(s1->s, s1->len, s2->s, s2->len, false);
}

/** \brief performs utf8-aware, case-sensitive equality check.
 */
static ALWAYS_INLINE bool lstr_utf8_equal2(const lstr_t s1, const lstr_t s2)
{
    return lstr_utf8_equal(&s1, &s2);
}

/** \brief returns whether \v s starts with \v p, in a case-insensitive
 * utf8-aware way.
 */
static ALWAYS_INLINE
int lstr_utf8_istartswith(const lstr_t *s1, const lstr_t *s2)
{
    return utf8_str_istartswith(s1->s, s1->len, s2->s, s2->len);
}

/** \brief returns whether \v s starts with \v p, in a case-sensitive
 * utf8-aware way.
 */
static ALWAYS_INLINE
int lstr_utf8_startswith(const lstr_t *s1, const lstr_t *s2)
{
    return utf8_str_startswith(s1->s, s1->len, s2->s, s2->len);
}

/** \brief returns whether \v s ends with \v p, in a case-insensitive
 * utf8-aware way.
 */
int lstr_utf8_iendswith(const lstr_t *s1, const lstr_t *s2);

/** \brief returns whether \v s ends with \v p, in a case-sensitive
 * utf8-aware way.
 */
int lstr_utf8_endswith(const lstr_t *s1, const lstr_t *s2);

/* @func lstr_match_ctype
 * @param[in] s: The string to check.
 * @param[in] d: The ctype description.
 * @return a boolean if the string contains exclusively characters from the
 *         ctype description.
 */
static inline bool lstr_match_ctype(lstr_t s, const ctype_desc_t *d)
{
    for (int i = 0; i < s.len; i++) {
        if (!ctype_desc_contains(d, (unsigned char)s.s[i])) {
            return false;
        }
    }
    return true;
}

/** \brief returns the Damerau–Levenshtein distance between two strings.
 *
 * This is the number of additions, deletions, substitutions, and
 * transpositions needed to transform a string into another.
 * This can be used to detect possible misspellings in texts written by
 * humans.
 *
 * \param[in]  s1        the first string to compare.
 * \param[in]  s2        the second string to compare.
 * \param[in]  max_dist  the distance computation will be aborted if it is
 *                       detected that the result will be strictly greater
 *                       than this limit, allowing to save CPU time;
 *                       can be -1 for no limit.
 *
 * \return  -1 if \p max_dist was reached, the Damerau–Levenshtein distance
 *          between \p s1 and \p s2 otherwise.
 */
int lstr_dlevenshtein(const lstr_t s1, const lstr_t s2, int max_dist);

/** Retrieve the number of characters of an utf-8 encoded string.
 *
 * \return -1 in case of invalid utf-8 characters encountered.
 */
static inline int lstr_utf8_strlen(lstr_t s)
{
    return utf8_strnlen(s.s, s.len);
}

/** Truncate the string to the given number of utf8 characters.
 */
static inline lstr_t lstr_utf8_truncate(lstr_t s, int char_len)
{
    int pos = 0;

    while (char_len > 0 && pos < s.len) {
        if (utf8_ngetc_at(s.s, s.len, &pos) < 0) {
            return LSTR_NULL_V;
        }
        char_len--;
    }
    return LSTR_INIT_V(s.s, pos);
}

/* }}} */
/* Conversions {{{ */

/** \brief lower case the given lstr. Work only with ascii strings.
 */
static inline void lstr_ascii_tolower(lstr_t *s)
{
    for (int i = 0; i < s->len; i++)
        s->v[i] = tolower((unsigned char)s->v[i]);
}

/** \brief upper case the given lstr. Work only with ascii strings.
 */
static inline void lstr_ascii_toupper(lstr_t *s)
{
    for (int i = 0; i < s->len; i++)
        s->v[i] = toupper((unsigned char)s->v[i]);
}

/** \brief in-place reversing of the lstr.
 *
 * This function is not unicode aware.
 */
static inline void lstr_ascii_reverse(lstr_t *s)
{
    for (int i = 0; i < s->len / 2; i++) {
        SWAP(char, s->v[i], s->v[s->len - i - 1]);
    }
}

/** \brief  convert a lstr into an int.
 *
 *  \param  lstr the string to convert
 *  \param  out  pointer to the memory to store the result of the conversion
 *
 *  \result int
 *
 *  \retval  0   success
 *  \retval -1   failure (errno set)
 */
static inline int lstr_to_int(lstr_t lstr, int *out)
{
    int         tmp = errno;
    const byte *endp;

    lstr = lstr_rtrim(lstr);

    errno = 0;
    *out = memtoip(lstr.s, lstr.len, &endp);

    THROW_ERR_IF(errno);
    if (endp != (const byte *)lstr.s + lstr.len) {
        errno = EINVAL;
        return -1;
    }

    errno = tmp;

    return 0;
}

/** \brief  convert a lstr into an int64.
 *
 *  \param  lstr the string to convert
 *  \param  out  pointer to the memory to store the result of the conversion
 *
 *  \result int
 *
 *  \retval  0   success
 *  \retval -1   failure (errno set)
 */
static inline int lstr_to_int64(lstr_t lstr, int64_t *out)
{
    int         tmp = errno;
    const byte *endp;

    lstr = lstr_rtrim(lstr);

    errno = 0;
    *out = memtollp(lstr.s, lstr.len, &endp);

    THROW_ERR_IF(errno);
    if (endp != (const byte *)lstr.s + lstr.len) {
        errno = EINVAL;
        return -1;
    }

    errno = tmp;

    return 0;
}

/** \brief  convert a lstr into an uint64.
 *
 *  If the string begins with a minus sign (white spaces are skipped), the
 *  function returns -1 and errno is set to ERANGE.
 *
 *  \param  lstr the string to convert
 *  \param  out  pointer to the memory to store the result of the conversion
 *
 *  \result int
 *
 *  \retval  0   success
 *  \retval -1   failure (errno set)
 */
static inline int lstr_to_uint64(lstr_t lstr, uint64_t *out)
{
    int         tmp = errno;
    const byte *endp;

    lstr = lstr_trim(lstr);

    errno = 0;
    *out = memtoullp(lstr.s, lstr.len, &endp);

    THROW_ERR_IF(errno);
    if (endp != (const byte *)lstr.s + lstr.len) {
        errno = EINVAL;
        return -1;
    }

    errno = tmp;

    return 0;
}

/** \brief  convert a lstr into an uint32.
 *
 *  If the string begins with a minus sign (white spaces are skipped), the
 *  function returns -1 and errno is set to ERANGE.
 *
 *  \param  lstr the string to convert
 *  \param  out  pointer to the memory to store the result of the conversion
 *
 *  \result int
 *
 *  \retval  0   success
 *  \retval -1   failure (errno set)
 */
static inline int lstr_to_uint(lstr_t lstr, uint32_t *out)
{
    uint64_t u64;

    RETHROW(lstr_to_uint64(lstr, &u64));

    if (u64 > UINT32_MAX) {
        errno = ERANGE;
        return -1;
    }

    *out = u64;
    return 0;
}

/** \brief  convert a lstr into an double.
 *
 *  \param  lstr the string to convert
 *  \param  out  pointer to the memory to store the result of the conversion
 *
 *  \result int
 *
 *  \retval  0   success
 *  \retval -1   failure (errno set)
 */
static inline int lstr_to_double(lstr_t lstr, double *out)
{
    int         tmp = errno;
    const byte *endp;

    lstr = lstr_rtrim(lstr);

    errno = 0;
    *out = memtod(lstr.s, lstr.len, &endp);

    THROW_ERR_IF(errno);
    if (endp != (const byte *)lstr.s + lstr.len) {
        errno = EINVAL;
        return -1;
    }

    errno = tmp;

    return 0;
}

/** \brief  Decode a hexadecimal lstr
 *
 *  \param  lstr      the hexadecimal string to convert
 *
 *  \result lstr_t    the result of the conversion (allocated on t_stack)
 *
 *  \retval LSTR_NULL failure
 */
static inline lstr_t t_lstr_hexdecode(lstr_t lstr)
{
    char *s;
    int len;

    len = lstr.len / 2;
    s   = t_new_raw(char, len + 1);

    if (strconv_hexdecode(s, len, lstr.s, lstr.len) < 0) {
        return LSTR_NULL_V;
    }

    s[len] = '\0';
    return LSTR_INIT_V(s, len);
}

/** \brief  Enccode a lstr into hexadecimal
 *
 *  \param  lstr        the string to convert
 *
 *  \result lstr_t      the result of the conversion (allocated on t_stack)
 *
 *  \retval  LSTR_NULL  failure
 */
static inline lstr_t t_lstr_hexencode(lstr_t lstr)
{
    char *s;
    int len;

    len = lstr.len * 2;
    s   = t_new_raw(char, len + 1);

    if (strconv_hexencode(s, len + 1, lstr.s, lstr.len) < 0) {
        return LSTR_NULL_V;
    }

    return LSTR_INIT_V(s, len);
}

/* }}} */
/* Format {{{ */

#define lstr_fmt(fmt, ...) \
    ({ const char *__s = asprintf(fmt, ##__VA_ARGS__); \
       lstr_init_(__s, strlen(__s), MEM_LIBC); })

#define mp_lstr_fmt(mp, fmt, ...) \
    ({ int __len; const char *__s = mp_fmt(mp, &__len, fmt, ##__VA_ARGS__); \
       mp_lstr_init((mp), __s, __len); })

#define t_lstr_fmt(fmt, ...)  mp_lstr_fmt(t_pool(), fmt, ##__VA_ARGS__)


#define lstr_vfmt(fmt, va) \
    ({ const char *__s = vasprintf(fmt, va); \
       lstr_init_(__s, strlen(__s), MEM_LIBC); })

#define mp_lstr_vfmt(mp, fmt, va) \
    ({ int __len; const char *__s = mp_vfmt(mp, &__len, fmt, va); \
       mp_lstr_init((mp), __s, __len); })

#define t_lstr_vfmt(fmt, va)  mp_lstr_vfmt(t_pool(), fmt, va)


/* }}} */
#endif
