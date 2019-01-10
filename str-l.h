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

#if !defined(IS_LIB_COMMON_STR_H) || defined(IS_LIB_COMMON_STR_L_H)
#  error "you must include str.h instead"
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
    };
    int     len;
    flag_t  mem_pool : 2;
} lstr_t;

#define LSTR_INIT(s_, len_)   { { (s_) }, (len_), 0 }
#define LSTR_INIT_V(s, len)   (lstr_t)LSTR_INIT(s, len)
#define LSTR_IMMED(str)       LSTR_INIT(""str, sizeof(str) - 1)
#define LSTR_IMMED_V(str)     LSTR_INIT_V(""str, sizeof(str) - 1)
#define LSTR_STR_V(str)       ({ const char *__s = (str); LSTR_INIT_V(__s, strlen(__s)); })
#define LSTR_NULL             LSTR_INIT(NULL, 0)
#define LSTR_NULL_V           LSTR_INIT_V(NULL, 0)
#define LSTR_EMPTY            LSTR_INIT("", 0)
#define LSTR_EMPTY_V          LSTR_INIT_V("", 0)
#define LSTR_SB(sb)           LSTR_INIT((sb)->data, (sb)->len)
#define LSTR_SB_V(sb)         LSTR_INIT_V((sb)->data, (sb)->len)
#define LSTR_FMT_ARG(s_)      (s_).len, (s_).s

/* obsolete stuff, please try not to use anymore */
#define clstr_t        lstr_t
#define clstr_equal    lstr_equal
#define clstr_equal2   lstr_equal2
#define CLSTR_INIT     LSTR_INIT
#define CLSTR_INIT_V   LSTR_INIT_V
#define CLSTR_IMMED    LSTR_IMMED
#define CLSTR_IMMED_V  LSTR_IMMED_V
#define CLSTR_STR_V    LSTR_STR_V
#define CLSTR_NULL     LSTR_NULL
#define CLSTR_NULL_V   LSTR_NULL_V
#define CLSTR_EMPTY    LSTR_EMPTY
#define CLSTR_EMPTY_V  LSTR_EMPTY_V
#define CLSTR_SB       LSTR_SB
#define CLSTR_SB_V     LSTR_SB_V


/*--------------------------------------------------------------------------*/
/** \brief lstr_dup_* helper.
 */
static ALWAYS_INLINE lstr_t lstr_init_(const void *s, int len, unsigned flags)
{
    return (lstr_t){ { (const char *)s }, len, flags };
}

/** \brief lstr_copy_* helper.
 */
static ALWAYS_INLINE
void lstr_copy_(mem_pool_t *mp, lstr_t *out,
                const void *s, int len, unsigned flags)
{
    if (mp && out->mem_pool == MEM_OTHER) {
        mp_delete(mp, &out->v);
    } else {
        ifree(out->v, out->mem_pool);
    }
    *out = lstr_init_(s, len, flags);
}


/*--------------------------------------------------------------------------*/
/** \brief wipe a lstr_t (frees memory if needed).
 */
static inline void lstr_wipe(lstr_t *s)
{
    lstr_copy_(NULL, s, NULL, 0, MEM_STATIC);
}

/** \brief wipe a lstr_t (frees memory if needed).
 * This flavour assumes that the passed memory pool is the one to deallocate
 * from if the lstr_t is known as beeing allocated in a pool.
 */
static inline void mp_lstr_wipe(mem_pool_t *mp, lstr_t *s)
{
    if (s->mem_pool == MEM_OTHER) {
        mp_delete(mp, &s->v);
        *s = lstr_init_(NULL, 0, MEM_STATIC);
    } else {
        lstr_wipe(s);
    }
}

/** \brief copies \v src into \dst tranferring memory ownership to \v dst.
 */
static inline void lstr_transfer(lstr_t *dst, lstr_t *src)
{
    lstr_copy_(NULL, dst, src->s, src->len, src->mem_pool);
    src->mem_pool = MEM_STATIC;
}


/*--------------------------------------------------------------------------*/
/** \brief returns "memcmp" ordering of \v s1 and \v s2.
 */
static ALWAYS_INLINE int lstr_cmp(const lstr_t *s1, const lstr_t *s2)
{
    int len = MIN(s1->len, s2->len);
    return memcmp(s1->s, s2->s, len) ?: CMP(s1->len, s2->len);
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

/** \brief returns whether \v s starts with \v p
 */
static ALWAYS_INLINE bool lstr_startswith(const lstr_t s, const lstr_t p)
{
    return s.len >= p.len && memcmp(s.s, p.s, p.len) == 0;
}

/** \brief returns whether \c s ends with \c p.
 */
static ALWAYS_INLINE bool lstr_endswith(const lstr_t s, const lstr_t p)
{
    return s.len >= p.len && memcmp(s.s + s.len - p.len, p.s, p.len) == 0;
}

/** \brief performs utf8-aware, case-insensitive comparison.
 */
static ALWAYS_INLINE int lstr_utf8_icmp(const lstr_t *s1, const lstr_t *s2)
{
    return utf8_stricmp(s1->s, s1->len, s2->s, s2->len, false);
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

/*--------------------------------------------------------------------------*/
/** \brief returns a constant copy of \v s.
 */
static inline lstr_t lstr_dupc(const lstr_t s)
{
    return lstr_init_(s.s, s.len, MEM_STATIC);
}

/** \brief returns new libc allocated lstr from its arguments.
 */
static inline lstr_t lstr_dups(const char *s, int len)
{
    if (!s)
        return LSTR_NULL_V;
    return lstr_init_(p_dupz(s, len), len, MEM_LIBC);
}

/** \brief returns new libc allocated lstr from its arguments.
 */
static inline lstr_t lstr_dup(const lstr_t s)
{
    if (!s.s)
        return LSTR_NULL_V;
    return lstr_init_(p_dupz(s.s, s.len), s.len, MEM_LIBC);
}

/** \brief returns new \v mp allocated lstr from its arguments.
 */
static inline lstr_t mp_lstr_dups(mem_pool_t *mp, const char *s, int len)
{
    if (!s)
        return LSTR_NULL_V;
    return lstr_init_(mp_dupz(mp, s, len), len, MEM_OTHER);
}

/** \brief returns new \v mp allocated lstr from its arguments.
 */
static inline lstr_t mp_lstr_dup(mem_pool_t *mp, const lstr_t s)
{
    if (!s.s)
        return LSTR_NULL_V;
    return lstr_init_(mp_dupz(mp, s.s, s.len), s.len, MEM_OTHER);
}


/*--------------------------------------------------------------------------*/
/** \brief copies a constant of \v s into \v out.
 */
static inline void lstr_copyc(lstr_t *out, const lstr_t s)
{
    return lstr_copy_(NULL, out, s.s, s.len, MEM_STATIC);
}

/** \brief sets \v dst to a new libc allocated lstr from its arguments.
 */
static inline void lstr_copys(lstr_t *dst, const char *s, int len)
{
    lstr_copy_(NULL, dst, p_dupz(s, len), len, MEM_LIBC);
}

/** \brief sets \v dst to a new libc allocated lstr from its arguments.
 */
static inline void lstr_copy(lstr_t *dst, const lstr_t src)
{
    lstr_copy_(NULL, dst, p_dupz(src.s, src.len), src.len, MEM_LIBC);
}

/** \brief sets \v dst to a new \v mp allocated lstr from its arguments.
 */
static inline
void mp_lstr_copys(mem_pool_t *mp, lstr_t *dst, const char *s, int len)
{
    lstr_copy_(mp, dst, mp_dupz(mp, s, len), len, MEM_OTHER);
}

/** \brief sets \v dst to a new \v mp allocated lstr from its arguments.
 */
static inline void mp_lstr_copy(mem_pool_t *mp, lstr_t *dst, const lstr_t src)
{
    lstr_copy_(mp, dst, mp_dupz(mp, src.s, src.len), src.len, MEM_OTHER);
}


/*--------------------------------------------------------------------------*/
/** \brief returns a duplicated lstr from the mem stack.
 */
static inline lstr_t t_lstr_dup(const lstr_t s)
{
    return lstr_init_(t_dupz(s.s, s.len), s.len, MEM_STACK);
}

/** \brief returns a duplicated lstr from the mem stack.
 */
static inline lstr_t t_lstr_dups(const void *s, int len)
{
    return lstr_init_(t_dupz(s, len), len, MEM_STACK);
}

/** \brief sets \v out to a mem stack allocated copy of its arguments.
 */
static inline void t_lstr_copys(lstr_t *out, const void *s, int len)
{
    return lstr_copy_(NULL, out, t_dupz(s, len), len, MEM_STACK);
}

/** \brief sets \v out to a mem stack allocated copy of its arguments.
 */
static inline void t_lstr_copy(lstr_t *out, const lstr_t s)
{
    return lstr_copy_(NULL, out, t_dupz(s.s, s.len), s.len, MEM_STACK);
}

/** \brief concatenates its argument to form a new lstr on the mem stack.
 */
static inline lstr_t t_lstr_cat(const lstr_t s1, const lstr_t s2)
{
    int    len = s1.len + s2.len;
    lstr_t res = lstr_init_(t_new_raw(char, len + 1), len, MEM_STACK);
    void  *s   = (void *)res.v;

    s = mempcpy(s, s1.s, s1.len);
    mempcpyz(s, s2.s, s2.len);
    return res;
}

/** \brief concatenates its argument to form a new lstr on the mem stack.
 */
static inline lstr_t t_lstr_cat3(const lstr_t s1, const lstr_t s2, const lstr_t s3)
{
    int    len = s1.len + s2.len + s3.len;
    lstr_t res = lstr_init_(t_new_raw(char, len + 1), len, MEM_STACK);
    void  *s   = (void *)res.v;

    s = mempcpy(s, s1.s, s1.len);
    s = mempcpy(s, s2.s, s2.len);
    mempcpyz(s, s3.s, s3.len);
    return res;
}

/** \brief return the trimmed lstr.
 */
static inline lstr_t lstr_trim(lstr_t s)
{
    /* ltrim */
    while (s.len && isspace((unsigned char)s.s[0])) {
        s.s++;
        s.len--;
    }

    /* rtrim */
    while (s.len && isspace((unsigned char)s.s[s.len - 1]))
        s.len--;

    return s;
}

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

/*--------------------------------------------------------------------------*/
#define lstr_fmt(fmt, ...) \
    ({ const char *__s = asprintf(fmt, ##__VA_ARGS__); \
       lstr_init_(__s, strlen(__s), MEM_LIBC); })

#define t_lstr_fmt(fmt, ...) \
    ({ int __len; const char *__s = t_fmt(&__len, fmt, ##__VA_ARGS__); \
       lstr_init_(__s, __len, MEM_STACK); })

#endif
