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

#define memcpy_check_types(e1, e2) \
    STATIC_ASSERT(                                                           \
        __builtin_choose_expr(                                               \
            __builtin_choose_expr(                                           \
                __builtin_types_compatible_p(typeof(e2), void *),            \
                true, sizeof(*(e2)) == 1), true,                             \
            __builtin_types_compatible_p(typeof(*(e1)), typeof(*(e2)))))

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


#endif
