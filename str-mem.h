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

#if !defined(IS_LIB_COMMON_STR_H) || defined(IS_LIB_COMMON_STR_MEM_H)
#  error "you must include str.h instead"
#else
#define IS_LIB_COMMON_STR_MEM_H

#ifndef __USE_GNU
static inline void *mempcpy(void *dst, const void *src, size_t n) {
    return memcpy(dst, src, n) + n;
}
#endif

static inline void *memcpyz(void *dst, const void *src, size_t n) {
    *(char *)mempcpy(dst, src, n) = '\0';
    return dst;
}
static inline void *mempcpyz(void *dst, const void *src, size_t n) {
    dst = mempcpy(dst, src, n);
    *(char *)dst = '\0';
    return (char *)dst + 1;
}

#ifdef __cplusplus
#define memcpy_check_types(e1, e2) \
    STATIC_ASSERT(sizeof(*(e1)) == sizeof(*(e1)))
#else
#define memcpy_check_types(e1, e2) \
    STATIC_ASSERT(                                                           \
        __builtin_choose_expr(                                               \
            __builtin_choose_expr(                                           \
                __builtin_types_compatible_p(typeof(e2), void *),            \
                true, sizeof(*(e2)) == 1), true,                             \
            __builtin_types_compatible_p(typeof(*(e1)), typeof(*(e2)))))
#endif

#define p_move(to, src, n) \
    ({ memcpy_check_types(to, src);                                          \
       (typeof(*(to)) *)memmove(to, src, sizeof(*(to)) * (n)); })
#define p_copy(to, src, n) \
    ({ memcpy_check_types(to, src);                                          \
       (typeof(*(to)) *)memcpy(to, src, sizeof(*(to)) * (n)); })
#define p_pcopy(to, src, n) \
    ({ memcpy_check_types(to, src);                                          \
       (typeof(*(to)) *)mempcpy(to, src, sizeof(*(to)) * (n)); })

#define p_move2(p, to, from, n) \
    ({ typeof(*(p)) *__p = (p); p_move(__p + (to), __p + (from), n); })
#define p_copy2(p, to, from, n) \
    ({ typeof(*(p)) __p = (p); p_copy(__p + (to), __p + (from), n); })


/** Copies at <code>n</code> characters from the string pointed to by
 * <code>src</code> to the buffer <code>dest</code> of <code>size</code>
 * bytes.
 * If <code>size</code> is greater than 0, a terminating '\0' character will
 * be put at the end of the copied string.
 *
 * The source and destination strings should not overlap.  No assumption
 * should be made on the values of the characters after the first '\0'
 * character in the destination buffer.
 *
 * @return <code>n</code>
 */
size_t pstrcpymem(char *dest, ssize_t size, const void *src, size_t n)
    __leaf;

/** Copies the string pointed to by <code>src</code> to the buffer
 * <code>dest</code> of <code>size</code> bytes.
 * If <code>size</code> is greater than 0, a terminating '\0' character will
 * be put at the end of the copied string.
 *
 * The source and destination strings should not overlap.  No assumption
 * should be made on the values of the characters after the first '\0'
 * character in the destination buffer.
 *
 * @return the length of the source string.
 * @see pstrcpylen
 *
 * RFE/RFC: In a lot of cases, we do not care that much about the length of
 * the source string. What we want is "has the string been truncated
 * and we should stop here, or is everything fine ?". Therefore, may be
 * we need a function like :
 * int pstrcpy_ok(char *dest, ssize_t size, const char *src)
 * {
 *     if (pstrcpy(dest, size, src) < size) {
 *        return 0;
 *     } else {
 *        return 1;
 *     }
 * }
 *
 */
__attr_nonnull__((1, 3))
static ALWAYS_INLINE
size_t pstrcpy(char *dest, ssize_t size, const char *src)
{
    return pstrcpymem(dest, size, src, strlen(src));
}

/** Copies at most <code>n</code> characters from the string pointed
 * to by <code>src</code> to the buffer <code>dest</code> of
 * <code>size</code> bytes.
 * If <code>size</code> is greater than 0, a terminating '\0' character will
 * be put at the end of the copied string.
 *
 * The source and destination strings should not overlap.  No assumption
 * should be made on the values of the characters after the first '\0'
 * character in the destination buffer.
 *
 * @return the length of the source string or <code>n</code> if smaller.
 */
__attr_nonnull__((1, 3))
static ALWAYS_INLINE
size_t pstrcpylen(char *dest, ssize_t size, const char *src, size_t n)
{
    return pstrcpymem(dest, size, src, strnlen(src, n));
}

/** Appends the string pointed to by <code>src</code> at the end of
 * the string pointed to by <code>dest</code> not overflowing
 * <code>size</code> bytes.
 *
 * The source and destination strings should not overlap.
 * No assumption should be made on the values of the characters
 * after the first '\0' character in the destination buffer.
 *
 * If the destination buffer doesn't contain a properly '\0' terminated
 * string, dest is unchanged and the value returned is size+strlen(src).
 *
 * @return the length of the source string plus the length of the
 * destination string.
 */
__attr_nonnull__((1, 3))
static inline size_t
pstrcat(char *dest, ssize_t size, const char *src)
{
    size_t dlen = size > 0 ? strnlen(dest, size) : 0;
    return dlen + pstrcpy(dest + dlen, size - dlen, src);
}

#endif
