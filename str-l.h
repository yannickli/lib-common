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
        void       *data;
    };
    int     len;
    flag_t  mem_pool : 3;
} lstr_t;

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

#define LSTR_FMT_ARG(s_)      (s_).len, (s_).s

#define LSTR_OPT(str)         ({ const char *__s = (str);              \
                                 __s ? LSTR_INIT_V(__s, strlen(__s))   \
                                     : LSTR_NULL_V; })

/* obsolete stuff, please try not to use anymore */
#define LSTR_STR_V      LSTR
#define LSTR_OPT_STR_V  LSTR_OPT


/*--------------------------------------------------------------------------*/
/** \brief lstr_dup_* helper.
 */
static ALWAYS_INLINE lstr_t lstr_init_(const void *s, int len, unsigned flags)
{
    return (lstr_t){ { (const char *)s }, len, flags };
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
void lstr_copy_(mem_pool_t *mp, lstr_t *dst,
                const void *s, int len, unsigned flags)
{
    if (mp && dst->mem_pool == MEM_OTHER) {
        mp_delete(mp, &dst->v);
    } else
    if (dst->mem_pool == MEM_MMAP) {
        (lstr_munmap)(dst);
    } else {
        ifree(dst->v, dst->mem_pool);
    }
    *dst = lstr_init_(s, len, flags);
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

/** \brief returns whether \v s1 contains substring \v s2.
 */
static ALWAYS_INLINE bool lstr_contains(const lstr_t s1, const lstr_t s2)
{
    return memmem(s1.data, s1.len, s2.data, s2.len) != NULL;
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
            return false;
    }
    return true;
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
    if (len < 0)
        len = strlen(s);
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

/** \brief force lstr to be heap-allocated.
 *
 * This function ensure the lstr_t is allocated on the heap and thus is
 * guaranteed to be persistent.
 */
static inline void lstr_persists(lstr_t *s)
{
    assert (s->mem_pool != MEM_OTHER);
    if (s->mem_pool != MEM_LIBC) {
        s->s        = (char *)p_dupz(s->s, s->len);
        s->mem_pool = MEM_LIBC;
    }
}

/** \brief returns new \v mp allocated lstr from its arguments.
 */
static inline lstr_t mp_lstr_dups(mem_pool_t *mp, const char *s, int len)
{
    if (!s)
        return LSTR_NULL_V;
    if (len < 0)
        len = strlen(s);
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

/** \brief ensure \p s is \p mp or heap allocated.
 */
static inline void mp_lstr_persists(mem_pool_t *mp, lstr_t *s)
{
    if (s->mem_pool != MEM_LIBC && s->mem_pool != MEM_OTHER) {
        s->s        = (char *)mp_dupz(mp, s->s, s->len);
        s->mem_pool = MEM_OTHER;
    }
}


/*--------------------------------------------------------------------------*/
/** \brief copies a constant of \v s into \v dst.
 */
static inline void lstr_copyc(lstr_t *dst, const lstr_t s)
{
    lstr_copy_(NULL, dst, s.s, s.len, MEM_STATIC);
}

/** \brief sets \v dst to a new libc allocated lstr from its arguments.
 */
static inline void lstr_copys(lstr_t *dst, const char *s, int len)
{
    if (s) {
        if (len < 0)
            len = strlen(s);
        lstr_copy_(NULL, dst, p_dupz(s, len), len, MEM_LIBC);
    } else {
        lstr_copy_(NULL, dst, NULL, 0, MEM_STATIC);
    }
}

/** \brief sets \v dst to a new libc allocated lstr from its arguments.
 */
static inline void lstr_copy(lstr_t *dst, const lstr_t src)
{
    if (src.s) {
        lstr_copy_(NULL, dst, p_dupz(src.s, src.len), src.len, MEM_LIBC);
    } else {
        lstr_copy_(NULL, dst, NULL, 0, MEM_STATIC);
    }
}

/** \brief sets \v dst to a new \v mp allocated lstr from its arguments.
 */
static inline
void mp_lstr_copys(mem_pool_t *mp, lstr_t *dst, const char *s, int len)
{
    if (s) {
        if (len < 0)
            len = strlen(s);
        lstr_copy_(mp, dst, mp_dupz(mp, s, len), len, MEM_OTHER);
    } else {
        lstr_copy_(NULL, dst, NULL, 0, MEM_STATIC);
    }
}

/** \brief sets \v dst to a new \v mp allocated lstr from its arguments.
 */
static inline void mp_lstr_copy(mem_pool_t *mp, lstr_t *dst, const lstr_t src)
{
    if (src.s) {
        lstr_copy_(mp, dst, mp_dupz(mp, src.s, src.len), src.len, MEM_OTHER);
    } else {
        lstr_copy_(NULL, dst, NULL, 0, MEM_STATIC);
    }
}


/*--------------------------------------------------------------------------*/
/** \brief returns a duplicated lstr from the mem stack.
 */
static inline lstr_t t_lstr_dup(const lstr_t s)
{
    if (!s.s)
        return LSTR_NULL_V;
    return lstr_init_(t_dupz(s.s, s.len), s.len, MEM_STACK);
}

/** \brief returns a duplicated lstr from the mem stack.
 */
static inline lstr_t t_lstr_dups(const char *s, int len)
{
    if (!s)
        return LSTR_NULL_V;
    if (len < 0)
        len = strlen(s);
    return lstr_init_(t_dupz(s, len), len, MEM_STACK);
}

/** \brief sets \v dst to a mem stack allocated copy of its arguments.
 */
static inline void t_lstr_copys(lstr_t *dst, const char *s, int len)
{
    if (s) {
        if (len < 0)
            len = strlen(s);
        lstr_copy_(NULL, dst, t_dupz(s, len), len, MEM_STACK);
    } else {
        lstr_copy_(NULL, dst, NULL, 0, MEM_STATIC);
    }
}

/** \brief sets \v dst to a mem stack allocated copy of its arguments.
 */
static inline void t_lstr_copy(lstr_t *dst, const lstr_t s)
{
    if (s.s) {
        lstr_copy_(NULL, dst, t_dupz(s.s, s.len), s.len, MEM_STACK);
    } else {
        lstr_copy_(NULL, dst, NULL, 0, MEM_STATIC);
    }
}

/** \brief duplicates \p v on the t_stack and reverse its content.
 *
 * This function is not unicode-aware.
 */
static inline lstr_t t_lstr_dup_ascii_reversed(const lstr_t v)
{
    char *str;

    if (!v.s) {
        return v;
    }

    str = t_new_raw(char, v.len + 1);

    for (int i = 0; i < v.len; i++) {
        str[i] = v.s[v.len - i - 1];
    }
    str[v.len] = '\0';

    return lstr_init_(str, v.len, MEM_STACK);
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
    int prev_off = 0;
    char *str;

    if (!v.s) {
        return v;
    }

    str = t_new_raw(char, v.len + 1);
    while (prev_off < v.len) {
        int off = prev_off;
        int c = utf8_ngetc_at(v.s, v.len, &off);

        if (unlikely(c < 0)) {
            return LSTR_NULL_V;
        }
        memcpy(str + v.len - off, v.s + prev_off, off - prev_off);
        prev_off = off;
    }
    return lstr_init_(str, v.len, MEM_STACK);
}

/** \brief concatenates its argument to form a new lstr on the mem stack.
 */
static inline lstr_t t_lstr_cat(const lstr_t s1, const lstr_t s2)
{
    int    len;
    lstr_t res;
    void  *s;

    if (unlikely(!s1.s && !s2.s))
        return LSTR_NULL_V;

    len = s1.len + s2.len;
    res = lstr_init_(t_new_raw(char, len + 1), len, MEM_STACK);
    s = (void *)res.v;
    s = mempcpy(s, s1.s, s1.len);
    mempcpyz(s, s2.s, s2.len);
    return res;
}

/** \brief concatenates its argument to form a new lstr on the mem stack.
 */
static inline lstr_t t_lstr_cat3(const lstr_t s1, const lstr_t s2, const lstr_t s3)
{
    int    len;
    lstr_t res;
    void  *s;

    if (unlikely(!s1.s && !s2.s && !s3.s))
        return LSTR_NULL_V;

    len = s1.len + s2.len + s3.len;
    res = lstr_init_(t_new_raw(char, len + 1), len, MEM_STACK);
    s = (void *)res.v;
    s = mempcpy(s, s1.s, s1.len);
    s = mempcpy(s, s2.s, s2.len);
    mempcpyz(s, s3.s, s3.len);
    return res;
}

/** \brief return the left-trimmed lstr.
 */
__attribute__((warn_unused_result))
static inline lstr_t lstr_ltrim(lstr_t s)
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
__attribute__((warn_unused_result))
static inline lstr_t lstr_rtrim(lstr_t s)
{
    while (s.len && isspace((unsigned char)s.s[s.len - 1])) {
        s.len--;
    }

    return s;
}

/** \brief return the trimmed lstr.
 */
__attribute__((warn_unused_result))
static inline lstr_t lstr_trim(lstr_t s)
{
    return lstr_rtrim(lstr_ltrim(s));
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

/*--------------------------------------------------------------------------*/
#define lstr_fmt(fmt, ...) \
    ({ const char *__s = asprintf(fmt, ##__VA_ARGS__); \
       lstr_init_(__s, strlen(__s), MEM_LIBC); })

#define t_lstr_fmt(fmt, ...) \
    ({ int __len; const char *__s = t_fmt(&__len, fmt, ##__VA_ARGS__); \
       lstr_init_(__s, __len, MEM_STACK); })

#define mp_lstr_fmt(mp, fmt, ...) \
    ({ int __len; const char *__s = mp_fmt(mp, &__len, fmt, ##__VA_ARGS__); \
       lstr_init_(__s, __len, MEM_OTHER); })

#endif
