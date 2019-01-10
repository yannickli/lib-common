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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_MACROS_H)
#  error "you must include core.h instead"
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

#if !defined(__doxygen_mode__)
#  if (!defined(__GNUC__) || __GNUC__ < 3) && !defined(__attribute__)
#    define __attribute__(attr)
#    define __must_be_array(a)   (void)0
#  else
#    define __must_be_array(a) \
         (sizeof(char[1 - 2 * __builtin_types_compatible_p(typeof(a), typeof(&(a)[0]))]) - 1)
#  endif

/*
 * __attr_unused__             => unused vars
 * __attr_noreturn__           => functions that perform abord()/exit()
 * __attr_printf__(pos_fmt, pos_first_arg) => printf format
 * __attr_scanf__(pos_fmt, pos_first_arg) => scanf format
 */
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
#  ifdef __OPTIMIZE__
#    if __GNUC_PREREQ(4, 3)
#      define ALWAYS_INLINE inline __attribute__((always_inline,artificial))
#    else
#      define ALWAYS_INLINE inline __attribute__((always_inline))
#    endif
#  else
#    define ALWAYS_INLINE inline
#  endif
#  define NEVER_INLINE __attribute__((noinline))
#  if __GNUC_PREREQ(4, 3)
#    define __cold __attribute__((cold))
#  else
#    define __cold
#  endif
#  if __GNUC_PREREQ(4, 1)
#    define __flatten __attribute__((flatten))
#  else
#    define __flatten
#  endif
#else
#  ifndef EXPORT
#    define EXPORT  extern
#  endif
#  define HIDDEN    extern
#  define ALWAYS_INLINE inline
#  define NEVER_INLINE __attribute__((noinline))
#endif


/** \def STATIC_ASSERT
 * \brief Check a condition at build time.
 * \param[in]  expr    the expression you want to be always true at compile * time.
 * \safemacro
 *
 */
#ifdef __GNUC__
#  define __error__(msg)          (void)({__asm__(".error \""msg"\"");})
#  define STATIC_ASSERT(cond) \
      __builtin_choose_expr(cond, (void)0, \
                            __error__("static assertion failed: "#cond""))
#  define ASSERT_COMPATIBLE(e1, e2) \
    STATIC_ASSERT(__builtin_types_compatible_p(typeof(e1), typeof(e2)))
#  define likely(expr)    __builtin_expect(!!(expr), 1)
#  define unlikely(expr)  __builtin_expect((expr), 0)
#  define prefetch(addr)   __builtin_prefetch(addr)
#  define prefetchw(addr)  __builtin_prefetch(addr, 1)
#else
#  define __error__(msg)            0
#  define STATIC_ASSERT(condition)  ((void)sizeof(char[1 - 2 * !(condition)]))
#  define ASSERT_COMPATIBLE(e1, e2)
#  define likely(expr)    expr
#  define unlikely(expr)  expr
#  define prefetch(addr)  (void)0
#  define prefetchw(addr) (void)0
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

#undef __acquires
#undef __releases
#undef __needlock

#if 0 && defined(__cplusplus)
#  define cast(type, v)    reinterpret_cast<type>(v)
#else
#  define cast(type, v)    ((type)(v))
#endif

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

/*---------------- useful expressions ----------------*/

#ifndef MAX
#define MAX(a,b)     (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b)     (((a) > (b)) ? (b) : (a))
#endif
#define MIN3(a,b,c)  (((a) > (b)) ? MIN(b, c) : MIN(a, c))
#define MAX3(a,b,c)  (((a) > (b)) ? MAX(a, c) : MAX(b, c))

#define CLIP(v,m,M)  (((v) > (M)) ? (M) : ((v) < (m)) ? (m) : (v))

#define DIV_ROUND_UP(x, y)   (((x) + (y) - 1) / (y))
#define ROUND_UP(x, y)       (DIV_ROUND_UP(x, y) * (y))

#define NEXTARG(argc, argv)  (argc--, *argv++)

#define RETHROW(e)        \
    ({ typeof(e) __res = (e);                          \
       if (unlikely(__res < 0))                        \
           return __res;                               \
       __res = __res;                                  \
    })

#define RETHROW_P(e)        \
    ({ typeof(*(e)) *__res = (e);                      \
       if (unlikely(__res == NULL))                    \
           return NULL;                                \
       __res;                                          \
    })

#define RETHROW_PN(e)        \
    ({ typeof(*(e)) *__res = (e);                      \
       if (unlikely(__res == NULL))                    \
           return -1;                                  \
       __res;                                          \
    })

#define RETHROW_NP(e)        \
    ({ typeof(e) __res = (e);                          \
       if (unlikely(__res < 0))                        \
           return NULL;                                \
       __res;                                          \
    })

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

#define CMP(x, y)     ((enum sign)(((x) > (y)) - ((x) < (y))))
#define CMP_LESS      NEGATIVE
#define CMP_EQUAL     ZERO
#define CMP_GREATER   POSITIVE
#define SIGN(x)       CMP(x, 0)

#define PAD4(len)     (((len) + 3) & ~3)
#define PAD4EXT(len)  (3 - (((len) - 1) & 3))

#define TOSTR_AUX(x)  #x
#define TOSTR(x)      TOSTR_AUX(x)

#define SWAP(typ, a, b)    do { typ __c = a; a = b; b = __c; } while (0)

#define get_unaligned_type(type_t, ptr)        \
    ({                                         \
        struct __attribute__((packed)) {       \
            type_t __v;                        \
        } *__p = (void *)(ptr);                \
        __p->__v;                              \
    })
#define get_unaligned(ptr)  get_unaligned_type(typeof(*(ptr)), ptr)

#define put_unaligned_type(type_t, ptr, v)     \
    ({                                         \
        struct __attribute__((packed)) {       \
            type_t __v;                        \
        } *__p = (void *)(ptr);                \
        type_t __v = (v);                      \
        __p->__v = __v;                        \
        __p + 1;                               \
    })
#define put_unaligned(ptr, v)  put_unaligned_type(typeof(v), ptr, v)

/*---------------- Types ----------------*/

typedef uint64_t cpu64_t;
typedef uint64_t __bitwise__ be64_t;
typedef uint64_t __bitwise__ le64_t;
typedef uint64_t __bitwise__ le48_t;
typedef uint64_t __bitwise__ be48_t;
typedef uint32_t cpu32_t;
typedef uint32_t __bitwise__ le32_t;
typedef uint32_t __bitwise__ be32_t;
typedef uint32_t __bitwise__ le24_t;
typedef uint32_t __bitwise__ be24_t;
typedef uint16_t cpu16_t;
typedef uint16_t __bitwise__ le16_t;
typedef uint16_t __bitwise__ be16_t;

#define MAKE64(hi, lo)  (((uint64_t)(uint32_t)(hi) << 32) | (uint32_t)(lo))

typedef unsigned char byte;
typedef unsigned int flag_t;    /* for 1 bit bitfields */

#define fieldsizeof(type_t, m)  sizeof(((type_t *)0)->m)
#define fieldtypeof(type_t, m)  typeof(((type_t *)0)->m)
#define countof(table)          ((ssize_t)(sizeof(table) / sizeof((table)[0]) \
                                           + __must_be_array(table)))
#define ssizeof(foo)            ((ssize_t)sizeof(foo))

#define bitsizeof(type_t)       (sizeof(type_t) * CHAR_BIT)
#define BITS_TO_ARRAY_LEN(type_t, nbits)  \
    DIV_ROUND_UP(nbits, bitsizeof(type_t))

#define BITMASK_NTH(type_t, n) ( (type_t)1 << ((n) & (bitsizeof(type_t) - 1)))
#define BITMASK_LT(type_t, n)  (BITMASK_NTH(type_t, n) - 1)
#define BITMASK_LE(type_t, n)  ((BITMASK_NTH(type_t, n) << 1) - 1)
#define BITMASK_GE(type_t, n)  (~(type_t)0 << ((n) & (bitsizeof(type_t) - 1)))
#define BITMASK_GT(type_t, n)  (BITMASK_GE(type_t, n) << 1)

#define OP_BIT(bits, n, shift, op) \
    ((bits)[(unsigned)(n) / (shift)] op BITMASK_NTH(typeof(*(bits)), n))
#define TST_BIT(bits, n)    OP_BIT(bits, n, bitsizeof(*(bits)), &  )
#define SET_BIT(bits, n)    (void)OP_BIT(bits, n, bitsizeof(*(bits)), |= )
#define RST_BIT(bits, n)    (void)OP_BIT(bits, n, bitsizeof(*(bits)), &=~)
#define CLR_BIT(bits, n)    RST_BIT(bits, n)
#define XOR_BIT(bits, n)    (void)OP_BIT(bits, n, bitsizeof(*(bits)), ^= )

#ifdef __GNUC__
#  define container_of(obj, type_t, member) \
      ({ const typeof(((type_t *)0)->member) *__mptr = (obj);              \
         (type_t *)((char *)__mptr - offsetof(type_t, member)); })
#else
#  define container_of(obj, type_t, member) \
      (type_t *)((char *)(obj) - offsetof(type_t, member))
#endif


/*---------------- Dangerous APIs ----------------*/

#undef sprintf
#define sprintf(...)  NEVER_USE_sprintf(__VA_ARGS__)
#undef strtok
#define strtok(...)   NEVER_USE_strtok(__VA_ARGS__)
#undef strncpy
#define strncpy(...)  NEVER_USE_strncpy(__VA_ARGS__)
#undef strncat
#define strncat(...)  NEVER_USE_strncat(__VA_ARGS__)

/** \} */
#endif
