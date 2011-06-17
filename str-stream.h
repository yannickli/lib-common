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

#if !defined(IS_LIB_COMMON_STR_H) || defined(IS_LIB_COMMON_STR_STREAM_H)
#  error "you must include str.h instead"
#else
#define IS_LIB_COMMON_STR_STREAM_H

/*
 * pstream_t's are basically a pointer and a length
 * and are meant to simplify parsing a lot
 *
 * Those structures are not meant to be allocated ever.
 */

/*
 * When you write a pstream function to parse a type_t, here is how you should
 * proceed:
 *
 * Case 1:
 * ~~~~~~
 *   When the size of the data is _easy_ to know before hand (we're reading a
 *   fixed amount of bytes for example) :
 *
 *   - depending on the fact that `type_t` is scalar or complex, provide:
 *     type_t __ps_get_type(pstream_t *ps, ...);
 *     void __ps_get_type(pstream_t *ps, type_t *res, ...);
 *
 *     This function shall assume that there is space to read the `type_t`.
 *
 *   - provide then a safe ps_get_type function looking like this:
 *     int ps_get_type(pstream_t *ps, type_t *res, ...) {
 *         if (unlikely(!ps_has(ps, EASY_LEN_TO_COMPUTE)))
 *             return -1;
 *          res = __ps_get_type(ps, ...);
 *          return 0;
 *     }
 *
 * Case 2:
 * ~~~~~~
 *   When the size of the data is hard to know, or too complex, you may only
 *   provide the safe interface. For example, ps_gets, we have to do a memchr
 *   to know the size, and it almost gives the result, you don't want to have
 *   both a __ps_gets and a ps_gets it's absurd.
 *
 *
 * In any case, if the task to perform is complex, do not inline the function,
 * and providing the unsafe version becomes useless too. The goal of the
 * unsafe function is to be able to inline the sizes checks.
 *
 */

typedef struct pstream_t {
    union {
        const char *s;
        const byte *b;
        const void *p;
    };
    union {
        const char *s_end;
        const byte *b_end;
        const void *p_end;
    };
} pstream_t;


/****************************************************************************/
/* init, checking constraints, skipping                                     */
/****************************************************************************/

#if 0
#define PS_WANT(c)   \
    do {                                                                    \
        if (unlikely(!(c))) {                                               \
            e_trace(0, "str-stream error on: %s != true", #c);              \
            return -1;                                                      \
        }                                                                   \
    } while (0)
#define PS_CHECK(c) \
    do {                                                                    \
        if (unlikely((c) < 0)) {                                            \
            e_trace(0, "str-stream error on: %s < 0", #c);                  \
            return -1;                                                      \
        }                                                                   \
    } while (0)
#else
#define PS_WANT(c)   do { if (unlikely(!(c)))    return -1; } while (0)
#define PS_CHECK(c)  RETHROW(c)
#endif

static inline pstream_t ps_initptr(const void *s, const void *p) {
    return (pstream_t){ { s }, { p } };
}
static inline pstream_t ps_init(const void *s, size_t len) {
    return ps_initptr(s, (const byte *)s + len);
}
static inline pstream_t ps_initstr(const char *s) {
    return ps_initptr(s, rawmemchr(s, '\0'));
}
static inline pstream_t ps_initsb(const sb_t *sb) {
    return ps_init(sb->data, sb->len);
}

static inline size_t ps_len(const pstream_t *ps) {
    return ps->s_end - ps->s;
}
static inline const void *ps_end(const pstream_t *ps) {
    return ps->p_end;
}

static inline bool ps_done(const pstream_t *ps) {
    return ps->p >= ps->p_end;
}
static inline bool ps_has(const pstream_t *ps, size_t len) {
    return ps->s + len <= ps->s_end;
}
static inline bool ps_contains(const pstream_t *ps, const void *p) {
    return p >= ps->p && p <= ps->p_end;
}

static inline bool ps_is_equal(pstream_t ps1, pstream_t ps2)
{
    return ps_len(&ps1) == ps_len(&ps2) && !memcmp(ps1.b, ps2.b, ps_len(&ps1));
}
static inline int ps_cmp(pstream_t ps1, pstream_t ps2)
{
    size_t len = MIN(ps_len(&ps1), ps_len(&ps2));
    return memcmp(ps1.b, ps2.b, len) ?: CMP(ps_len(&ps1), ps_len(&ps2));
}

static inline bool ps_startswith(const pstream_t *ps, const void *data, size_t len)
{
    return ps_len(ps) >= len && !memcmp(ps->p, data, len);
}
static inline bool ps_startswithstr(const pstream_t *ps, const char *s)
{
    return ps_startswith(ps, s, strlen(s));
}

static inline bool ps_memequal(const pstream_t *ps, const void *data, size_t len)
{
    return ps_len(ps) == len && !memcmp(ps->p, data, len);
}
static inline bool ps_strequal(const pstream_t *ps, const char *s) {
    return ps_memequal(ps, s, strlen(s));
}

/****************************************************************************/
/* skipping/trimming helpers                                                */
/****************************************************************************/

static inline int __ps_skip(pstream_t *ps, size_t len) {
    ps->s += len;
    return 0;
}
static inline int ps_skip(pstream_t *ps, size_t len) {
    return unlikely(!ps_has(ps, len)) ? -1 : __ps_skip(ps, len);
}
static inline int __ps_skip_upto(pstream_t *ps, const void *p) {
    ps->p = p;
    return 0;
}
static inline int ps_skip_upto(pstream_t *ps, const void *p) {
    PS_WANT(ps_contains(ps, p));
    return __ps_skip_upto(ps, p);
}

static inline int __ps_shrink(pstream_t *ps, size_t len) {
    ps->s_end -= len;
    return 0;
}
static inline int ps_shrink(pstream_t *ps, size_t len) {
    return unlikely(!ps_has(ps, len)) ? -1 : __ps_shrink(ps, len);
}

static inline int __ps_clip(pstream_t *ps, size_t len) {
    ps->s_end = ps->s + len;
    return 0;
}
static inline int ps_clip(pstream_t *ps, size_t len) {
    return unlikely(!ps_has(ps, len)) ? -1 : __ps_clip(ps, len);
}
static inline int __ps_clip_at(pstream_t *ps, const void *p) {
    ps->s_end = p;
    return 0;
}
static inline int ps_clip_at(pstream_t *ps, const void *p) {
    return unlikely(!ps_contains(ps, p)) ? -1 : __ps_clip_at(ps, p);
}

static inline int ps_skipdata(pstream_t *ps, const void *data, size_t len) {
    PS_WANT(ps_startswith(ps, data, len));
    return __ps_skip(ps, len);
}
static inline int ps_skipstr(pstream_t *ps, const char *s) {
    return ps_skipdata(ps, s, strlen(s));
}
static inline void ps_skipspaces(pstream_t *ps) {
    while (!ps_done(ps) && isspace(ps->b[0]))
        __ps_skip(ps, 1);
}
static inline int ps_skip_uptochr(pstream_t *ps, int c) {
    const char *p = memchr(ps->p, c, ps_len(ps));
    return likely(p) ? __ps_skip_upto(ps, p) : -1;
}
static inline int ps_skip_afterchr(pstream_t *ps, int c) {
    const char *p = memchr(ps->p, c, ps_len(ps));
    return likely(p) ? __ps_skip_upto(ps, p + 1) : -1;
}

/** \brief  Skips up to the (\a data, \a len) word
 * \return -1 if the word cannot be found
 */
static inline
int ps_skip_upto_data(pstream_t *ps, const void *data, size_t len)
{
    const char *mem = RETHROW_PN(memmem(ps->p, ps_len(ps), data, len));
    return __ps_skip_upto(ps, mem);
}

/** \brief  Skips after the (\a data, \a len) word
 * \return -1 if the word cannot be found
 */
static inline
int ps_skip_after_data(pstream_t *ps, const void *data, size_t len)
{
    const char *mem = RETHROW_PN(memmem(ps->p, ps_len(ps), data, len));
    return __ps_skip_upto(ps, mem + len);
}


/****************************************************************************/
/* extracting sub pstreams                                                  */
/****************************************************************************/
/*
 * extract means it doesn't modifies the "parent" ps
 * get means it reduces the size of the "parent" (skip or shrink)
 *
 */
static inline pstream_t __ps_extract_after(const pstream_t *ps, const void *p) {
    return ps_initptr(p, ps->p_end);
}
static inline int ps_extract_after(pstream_t *ps, const void *p, pstream_t *out) {
    PS_WANT(ps_contains(ps, p));
    *out = __ps_extract_after(ps, p);
    return 0;
}

static inline pstream_t __ps_get_ps_upto(pstream_t *ps, const void *p) {
    const void *old = ps->p;
    return ps_initptr(old, ps->p = p);
}
static inline int ps_get_ps_upto(pstream_t *ps, const void *p, pstream_t *out) {
    PS_WANT(ps_contains(ps, p));
    *out = __ps_get_ps_upto(ps, p);
    return 0;
}

static inline pstream_t __ps_get_ps(pstream_t *ps, size_t len) {
    const void *old = ps->b;
    return ps_initptr(old, ps->b += len);
}
static inline int ps_get_ps(pstream_t *ps, size_t len, pstream_t *out) {
    PS_WANT(ps_has(ps, len));
    *out = __ps_get_ps(ps, len);
    return 0;
}

static inline int ps_get_ps_chr(pstream_t *ps, int c, pstream_t *out) {
    const void *p = memchr(ps->s, c, ps_len(ps));
    PS_WANT(p);
    *out = __ps_get_ps_upto(ps, p);
    return 0;
}


/** \brief  Returns the bytes un tp the (\a d, \a len) word
 * \return -1 if the word cannot be found
 */
static inline int
ps_get_ps_upto_data(pstream_t *ps, const void *d, size_t len, pstream_t *out)
{
    const char *mem = RETHROW_PN(memmem(ps->p, ps_len(ps), d, len));
    *out = __ps_get_ps_upto(ps, mem);
    return 0;
}

/** \brief  Returns the bytes un tp the (\a data, \a len) word, and skip it.
 * \return -1 if the word cannot be found
 */
static inline int
ps_get_ps_upto_data_and_skip(pstream_t *ps,
                             const void *data, size_t len, pstream_t *out)
{
    const char *mem = RETHROW_PN(memmem(ps->p, ps_len(ps), data, len));
    *out = __ps_get_ps_upto(ps, mem);
    __ps_skip_upto(ps, mem + len);
    return 0;
}

/****************************************************************************/
/* copying helpers                                                          */
/****************************************************************************/

__attribute__((nonnull(1, 2, 3)))
int ps_copyv(pstream_t *ps, struct iovec *iov, size_t *iov_len, int *flags);


/****************************************************************************/
/* string parsing helpers                                                   */
/****************************************************************************/

static inline int ps_geti(pstream_t *ps) {
    return memtoip(ps->b, ps_len(ps), &ps->b);
}

static inline int64_t ps_getlli(pstream_t *ps) {
    return memtollp(ps->b, ps_len(ps), &ps->b);
}

static inline int __ps_getc(pstream_t *ps) {
    int c = *ps->b;
    __ps_skip(ps, 1);
    return c;
}

static inline int ps_getc(pstream_t *ps) {
    if (unlikely(!ps_has(ps, 1)))
        return EOF;
    return __ps_getc(ps);
}

static inline int ps_getuc(pstream_t *ps) {
    return utf8_ngetc(ps->s, ps_len(ps), &ps->s);
}

static inline int __ps_hexdigit(pstream_t *ps) {
    return hexdigit(__ps_getc(ps));
}
static inline int ps_hexdigit(pstream_t *ps) {
    PS_WANT(ps_has(ps, 1));
    return __ps_hexdigit(ps);
}

static inline int ps_hexdecode(pstream_t *ps) {
    int res;
    PS_WANT(ps_has(ps, 2));
    res = hexdecode(ps->s);
    __ps_skip(ps, 2);
    return res;
}

static inline const char *ps_gets(pstream_t *ps, int *len) {
    const char *end = memchr(ps->s, '\0', ps_len(ps));
    const char *res = ps->s;

    if (unlikely(!end))
        return NULL;
    if (len)
        *len = end - ps->s;
    __ps_skip(ps, end + 1 - ps->s);
    return res;
}

static inline int ps_skipcasedata(pstream_t *ps, const char *s, int len)
{
    PS_WANT(ps_has(ps, len));
    for (int i = 0; i < len; i++)
        PS_WANT(tolower(ps->s[i]) == s[i]);
    return __ps_skip(ps, len);
}

static inline int ps_skipcasestr(pstream_t *ps, const char *s)
{
    return ps_skipcasedata(ps, s, strlen(s));
}

static inline void ps_skip_span(pstream_t *ps, const ctype_desc_t *d)
{
    while (ps->b < ps->b_end && ctype_desc_contains(d, *ps->b))
        ps->b++;
}

static inline void ps_skip_cspan(pstream_t *ps, const ctype_desc_t *d)
{
    while (ps->b < ps->b_end && !ctype_desc_contains(d, *ps->b))
        ps->b++;
}

/* @func ps_get_span
 * @param[in] ps
 * @param[in] d
 * @return a sub pstream spanning on the first characters
 *         contained by d
 */
static inline pstream_t ps_get_span(pstream_t *ps, const ctype_desc_t *d)
{
    const byte *b = ps->b;

    while (b < ps->b_end && ctype_desc_contains(d, *b))
        b++;
    return __ps_get_ps_upto(ps, b);
}

/* @func ps_get_span
 * @param[in] ps
 * @param[in] d
 * @return a sub pstream spanning on the first characters
 *         not contained by d
 */
static inline pstream_t ps_get_cspan(pstream_t *ps, const ctype_desc_t *d)
{
    const byte *b = ps->b;

    while (b < ps->b_end && !ctype_desc_contains(d, *b))
        b++;
    return __ps_get_ps_upto(ps, b);
}

static inline pstream_t ps_get_tok(pstream_t *ps, const ctype_desc_t *d)
{
    pstream_t out = ps_get_cspan(ps, d);
    ps_skip_span(ps, d);
    return out;
}

static inline void ps_ltrim(pstream_t *ps) {
    while (ps->b < ps->b_end && isspace(ps->b[0]))
        ps->b++;
}
static inline void ps_rtrim(pstream_t *ps) {
    while (ps->b < ps->b_end && isspace(ps->b_end[-1]))
        ps->b_end--;
}
static inline void ps_trim(pstream_t *ps) {
    ps_ltrim(ps);
    ps_rtrim(ps);
}


/****************************************************************************/
/* binary parsing helpers                                                   */
/****************************************************************************/

/*
 * XXX: those are dangerous, be sure you won't trigger unaligned access !
 *
 * Also the code supposes that `align` is a power of 2. If it's not, then
 * results will be completely absurd.
 */

static inline bool ps_aligned(const pstream_t *ps, size_t align) {
    return ((uintptr_t)ps->p & (align - 1)) == 0;
}
#define ps_aligned2(ps)   ps_aligned(ps, 2)
#define ps_aligned4(ps)   ps_aligned(ps, 4)
#define ps_aligned8(ps)   ps_aligned(ps, 8)

static inline int __ps_align(pstream_t *ps, uintptr_t align) {
    return __ps_skip_upto(ps, (const void *)(((uintptr_t)ps->b + align - 1) & ~align));
}
static inline const void *__ps_get_block(pstream_t *ps, size_t len, size_t align) {
    const void *p = ps->p;
    __ps_skip(ps, (len + align - 1) & ~(align - 1));
    return p;
}
#define __ps_get_type(ps,  type_t)  ((type_t *)__ps_get_block(ps, sizeof(type_t), 1))
#define __ps_get_type2(ps, type_t)  ((type_t *)__ps_get_block(ps, sizeof(type_t), 2))
#define __ps_get_type4(ps, type_t)  ((type_t *)__ps_get_block(ps, sizeof(type_t), 4))
#define __ps_get_type8(ps, type_t)  ((type_t *)__ps_get_block(ps, sizeof(type_t), 8))

static inline int ps_align(pstream_t *ps, uintptr_t align) {
    const void *p = (const void *)(((uintptr_t)ps->b + align - 1) & ~align);
    PS_WANT(p <= ps->p_end);
    return __ps_skip_upto(ps, p);
}
static inline const void *ps_get_block(pstream_t *ps, size_t len, size_t align) {
    return unlikely(!ps_has(ps, len)) ? NULL : __ps_get_block(ps, len, align);
}
#define ps_get_type(ps,  type_t)    ((type_t *)ps_get_block(ps, sizeof(type_t), 1))
#define ps_get_type2(ps, type_t)    ((type_t *)ps_get_block(ps, sizeof(type_t), 2))
#define ps_get_type4(ps, type_t)    ((type_t *)ps_get_block(ps, sizeof(type_t), 4))
#define ps_get_type8(ps, type_t)    ((type_t *)ps_get_block(ps, sizeof(type_t), 8))


#endif
