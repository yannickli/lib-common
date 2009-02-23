/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_MACROS_H)
#  error "you must include <lib-common/core.h> instead"
#else
#define IS_LIB_COMMON_CORE_MACROS_H

/** \defgroup generic_macros Intersec Generic Macros.
 * \{
 */

/** \file core-macros.h
 * \brief Intersec generic macros.
 */

/*---------------- GNU extension wrappers ----------------*/

#ifndef __GNUC_PREREQ
#  if defined(__GNUC__) && defined(__GNUC_MINOR__)
#    define __GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#  else
#    define __GNUC_PREREQ(maj, min)   0
#  endif
#endif

/*
 * __attr_unused__             => unused vars
 * __attr_noreturn__           => functions that perform abord()/exit()
 * __attr_printf__(pos_fmt, pos_first_arg) => printf format
 * __attr_scanf__(pos_fmt, pos_first_arg) => scanf format
 */

/** \def STATIC_ASSERT
 * \brief Check a condition at build time.
 * \param[in]  expr    the expression you want to be always true at compile * time.
 * \safemacro
 *
 */
#ifdef __GNUC__
#define __error__(msg)          (void)({__asm__(".error \""msg"\"");})
#define STATIC_ASSERT(cond) \
    __builtin_choose_expr(cond, (void)0, __error__("static assertion failed: "#cond""))
#else
#define __error__(msg)          0
#define STATIC_ASSERT(condition) ((void)sizeof(char[1 - 2 * !(condition)]))
#endif

/** \brief Forcefully ignore the value of an expression.
 *
 * Use this to explicitly ignore return values of functions declared with
 * __attribute__((warn_unused_result)).
 *
 * \param[in]  expr    the expression.
 * \safemacro
 */
#define IGNORE(expr)             do { if (expr) (void)0; } while (0)

/** \brief Convenient functional macro that always expands to true.
 *
 * \warning This macro ignores all of its arguments. The arguments are
 *          not evaluated at all.
 */
#define TRUEFN(...)              true


#if !defined(__doxygen_mode__)
#  if (!defined(__GNUC__) || __GNUC__ < 3) && !defined(__attribute__)
#    define __attribute__(attr)
#    define __must_be_array(a)   (void)0
#  else
#    define __must_be_array(a) \
         (sizeof(char[1 - 2 * __builtin_types_compatible_p(typeof(a), typeof(&(a)[0]))]) - 1)
#  endif

#  define __unused__             __attribute__((unused))
#  define __must_check__         __attribute__((warn_unused_result))
#  define __attr_noreturn__      __attribute__((noreturn))
#  define __attr_nonnull__(l)    __attribute__((nonnull l))
#  define __attr_printf__(a, b)  __attribute__((format(printf, a, b)))
#  define __attr_scanf__(a, b)   __attribute__((format(scanf, a, b)))
#endif

#ifdef __GNUC__
#  ifndef EXPORT
#    define EXPORT  extern __attribute__((visibility("default")))
#  endif
#  define HIDDEN    extern __attribute__((visibility("hidden")))
#else
#  ifndef EXPORT
#    define EXPORT  extern
#  endif
#  define HIDDEN    extern
#endif

#ifdef __GNUC__
#  define likely(expr)    __builtin_expect(!!(expr), 1)
#  define unlikely(expr)  __builtin_expect((expr), 0)
#else
#  define likely(expr)    expr
#  define unlikely(expr)  expr
#endif

#undef __acquires
#undef __releases
#undef __needlock

#ifdef __SPARSE__
#  define __bitwise__   __attribute__((bitwise))
#  define force_cast(type, expr)    (__attribute__((force)) type)(expr)
#  define __acquires(x)  __attribute__((context(x, 0, 1)))
#  define __releases(x)  __attribute__((context(x, 1, 0)))
#  define __needlock(x)  __attribute__((context(x, 1, 1)))
#else
#  define __bitwise__
#  define force_cast(type, expr)    (type)(expr)
#  define __acquires(x)
#  define __releases(x)
#  define __needlock(x)
#endif

/*---------------- Types ----------------*/

typedef unsigned char byte;
typedef unsigned int flag_t;    /* for 1 bit bitfields */

/* OG: should find a better name such as BITSIZEOF(type) */
#define TYPE_BIT(type_t)    (sizeof(type_t) * CHAR_BIT)
#define BITS_TO_ARRAY_LEN(type_t, nbits)  \
    (((nbits) + TYPE_BIT(type_t) - 1) / TYPE_BIT(type_t))

#define OP_BIT(bits, n, shift, op) \
    ((bits)[(unsigned)(n) / (shift)] op (1 << ((n) & ((shift) - 1))))
#define TST_BIT(bits, n)  OP_BIT(bits, n, TYPE_BIT(*(bits)), &  )
#define SET_BIT(bits, n)  (void)OP_BIT(bits, n, TYPE_BIT(*(bits)), |= )
#define RST_BIT(bits, n)  (void)OP_BIT(bits, n, TYPE_BIT(*(bits)), &=~)
#define XOR_BIT(bits, n)  (void)OP_BIT(bits, n, TYPE_BIT(*(bits)), ^= )

/*---------------- Misc ----------------*/

#ifdef __GNUC__
#  define container_of(obj, type_t, member) \
      ({ const typeof(((type_t *)0)->member) *__mptr = (obj);              \
         (type_t *)((char *)__mptr - offsetof(type_t, member)); })
#  define prefetch(addr)   ({ __builtin_prefetch(addr); 1; })
#  define prefetchw(addr)  ({ __builtin_prefetch(addr, 1); 1; })
#else
#  define container_of(obj, type_t, member) \
      (type_t *)((char *)(obj) - offsetof(type_t, member))
#  define prefetch(addr)   1
#  define prefetchw(addr)  1
#endif


#define countof(table)  ((ssize_t)(sizeof(table) / sizeof((table)[0]) + \
                         __must_be_array(table)))
#define ssizeof(foo)    ((ssize_t)sizeof(foo))

#ifdef __SPARSE__ /* avoids lots of warning with this trivial hack */
#include <sys/param.h>
#endif
#ifndef MAX
#define MAX(a,b)     (((a) > (b)) ? (a) : (b))
#endif
#define MAX3(a,b,c)  (((a) > (b)) ? MAX(a, c) : MAX(b, c))

#define CLIP(v,m,M)  (((v) > (M)) ? (M) : ((v) < (m)) ? (m) : (v))

#define DIV_ROUND_UP(x, y)   (((x) + (y) - 1) / (y))
#define ROUND_UP(x, y)       (DIV_ROUND_UP(x, y) * (y))

#ifndef MIN
#define MIN(a,b)     (((a) > (b)) ? (b) : (a))
#endif
#define MIN3(a,b,c)  (((a) > (b)) ? MIN(b, c) : MIN(a, c))

#define NEXTARG(argc, argv)  (argc--, *argv++)

#ifdef CMP
#error CMP already defined
#endif
#ifdef SIGN
#error SIGN already defined
#endif

enum sign {
    NEGATIVE = -1,
    ZERO     = 0,
    POSITIVE = 1,
};

#define CMP(x, y)    ((enum sign)(((x) > (y)) - ((x) < (y))))
#define CMP_LESS     NEGATIVE
#define CMP_EQUAL    ZERO
#define CMP_GREATER  POSITIVE
#define SIGN(x)      CMP(x, 0)

#define PAD4(len)    (((len) + 3) & ~3)
#define PAD4EXT(len) (3 - (((len) - 1) & 3))

#define TOSTR_AUX(x)  #x
#define TOSTR(x)      TOSTR_AUX(x)

#define SWAP(typ, a, b)    do { typ __c = a; a = b; b = __c; } while (0)


#undef sprintf
#define sprintf(...)  NEVER_USE_sprintf(__VA_ARGS__)
#undef strtok
#define strtok(...)   NEVER_USE_strtok(__VA_ARGS__)
#undef strncpy
#define strncpy(...)  NEVER_USE_strncpy(__VA_ARGS__)
#undef strncat
#define strncat(...)  NEVER_USE_strncat(__VA_ARGS__)

__attr_nonnull__((1))
static inline int p_fclose(FILE **fpp) {
    FILE *fp = *fpp;

    *fpp = NULL;
    return fp ? fclose(fp) : 0;
}

__attr_nonnull__((1))
static inline int p_close(int *hdp) {
    int hd = *hdp;
    *hdp = -1;
    if (hd < 0)
        return 0;
    while (close(hd) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return 0;
}

/** \} */
#endif
