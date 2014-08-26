/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2014 INTERSEC SA                                   */
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
# error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_MACROS_H

/** \defgroup generic_macros Intersec Generic Macros.
 * \{
 */

/** \file core-macros.h
 * \brief Intersec generic macros.
 */

#ifndef __has_feature
# define __has_feature(x)  0
#endif
#ifndef __has_builtin
# define __has_builtin(x)  0
#endif
#ifndef __has_attribute
# define __has_attribute(x)  0
#endif

/*---------------- GNU extension wrappers ----------------*/

#if defined(__clang__)
# undef __GNUC_PREREQ
# define __GNUC_PREREQ(maj, min)  0
# define __CLANG_PREREQ(maj, min) \
    ((__clang_major__ << 16) + __clang_minor__ >= ((maj) << 16) + (min))
#else
# undef __CLANG_PREREQ
# define __CLANG_PREREQ(maj, min)  0
# if !defined(__GNUC_PREREQ) && defined(__GNUC__) && defined(__GNUC_MINOR__)
#   define __GNUC_PREREQ(maj, min) \
        ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
# elif !defined(__GNUC_PREREQ)
#   define __GNUC_PREREQ(maj, min)   0
# endif
#endif

#if !defined(__doxygen_mode__)
# if !__GNUC_PREREQ(3, 0) && !defined(__clang__)
#   define __attribute__(attr)
# endif
# if !defined(__GNUC__) || defined(__cplusplus)
#   define __must_be_array(a)   0
# else
#   define __must_be_array(a) \
         (sizeof(char[1 - 2 * __builtin_types_compatible_p(typeof(a), typeof(&(a)[0]))]) - 1)
# endif

#if defined(__has_feature)
# if  __has_feature(address_sanitizer)
#   define __has_asan       1
#   define __attr_noasan__  __attribute__((no_address_safety_analysis))
# endif
# if  __has_feature(thread_sanitizer)
#   define __has_tsan       1
#   define __attr_notsan__  __attribute__((no_sanitize_thread))
# endif
#endif
#ifndef __has_asan
# define __attr_noasan__
#endif
#ifndef __has_tsan
# define __attr_notsan__
#endif

/*
 * __attr_unused__             => unused vars
 * __attr_noreturn__           => functions that perform abord()/exit()
 * __attr_printf__(pos_fmt, pos_first_arg) => printf format
 * __attr_scanf__(pos_fmt, pos_first_arg) => scanf format
 */
# define __unused__             __attribute__((unused))
# define __must_check__         __attribute__((warn_unused_result))
# define __attr_noreturn__      __attribute__((noreturn))
# define __attr_nonnull__(l)    __attribute__((nonnull l))
# define __attr_printf__(a, b)  __attribute__((format(printf, a, b)))
# define __attr_scanf__(a, b)   __attribute__((format(scanf, a, b)))
#endif

#ifdef __GNUC__
# ifndef EXPORT
#   define EXPORT  extern __attribute__((visibility("default")))
# endif
# define HIDDEN    extern __attribute__((visibility("hidden")))
# ifdef __OPTIMIZE__
#   if __GNUC_PREREQ(4, 3) || __has_attribute(artificial)
#     define ALWAYS_INLINE inline __attribute__((always_inline,artificial))
#   else
#     define ALWAYS_INLINE inline __attribute__((always_inline))
#   endif
# else
#   define ALWAYS_INLINE inline
# endif
# define NEVER_INLINE __attribute__((noinline))
# if __GNUC_PREREQ(4, 5) || defined(__clang__)
#   define __deprecated__(msg)    __attribute__((deprecated(msg)))
# else
#   define __deprecated__(msg)    __attribute__((deprecated))
# endif
#else
# ifndef EXPORT
#   define EXPORT  extern
# endif
# define HIDDEN    extern
# define ALWAYS_INLINE inline
# define NEVER_INLINE __attribute__((noinline))
#endif

#if __GNUC_PREREQ(4, 1) || __has_attribute(flatten)
# define __flatten __attribute__((flatten))
#else
# define __flatten
#endif

#if __GNUC_PREREQ(4, 3) || __has_attribute(cold)
# define __cold __attribute__((cold))
#else
# define __cold
#endif

#if __GNUC_PREREQ(4, 6) || __has_attribute(leaf)
# define __leaf __attribute__((leaf))
#else
# define __leaf
#endif

#ifdef __GNUC__
# define likely(expr)     __builtin_expect(!!(expr), 1)
# define unlikely(expr)   __builtin_expect((expr), 0)
#else
# define likely(expr)     (expr)
# define unlikely(expr)   (expr)
#endif


/** \def STATIC_ASSERT
 * \brief Check a condition at build time.
 * \param[in]  expr    the expression you want to be always true at compile * time.
 * \safemacro
 *
 */
#if __has_feature(c_static_assert) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
# define __error__(msg)
# define STATIC_ASSERT(cond)  do { _Static_assert(cond, #cond); } while (0)
#   define ASSERT_COMPATIBLE(e1, e2) \
     STATIC_ASSERT(__builtin_types_compatible_p(typeof(e1), typeof(e2)))
#elif !defined(__cplusplus)
# ifdef __GNUC__
#   define __error__(msg)          (void)({__asm__(".error \""msg"\"");})
#   define STATIC_ASSERT(cond) \
       __builtin_choose_expr(cond, (void)0, \
                             __error__("static assertion failed: "#cond""))
#   define ASSERT_COMPATIBLE(e1, e2) \
     STATIC_ASSERT(__builtin_types_compatible_p(typeof(e1), typeof(e2)))
# else
#   define __error__(msg)            0
#   define STATIC_ASSERT(condition)  ((void)sizeof(char[1 - 2 * !(condition)]))
#   define ASSERT_COMPATIBLE(e1, e2)
# endif
#else
# define ASSERT_COMPATIBLE(e1, e2)
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
# define __bitwise__   __attribute__((bitwise))
# define force_cast(type, expr)    (__attribute__((force)) type)(expr)
# define __acquires(x)  __attribute__((context(x, 0, 1)))
# define __releases(x)  __attribute__((context(x, 1, 0)))
# define __needlock(x)  __attribute__((context(x, 1, 1)))
#else
# define __bitwise__
# define force_cast(type, expr)    (expr)
# define __acquires(x)
# define __releases(x)
# define __needlock(x)
#endif

#ifdef __APPLE__
# define __attr_section(sg, sc)  __attribute__((section(sg","sc)))
#else
# define __attr_section(sg, sc)  __attribute__((section("."sg"."sc)))
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
       __res;                                          \
    })

#define RETHROW_P(e)        \
    ({ typeof(e) __res = (e);                          \
       if (unlikely(__res == NULL))                    \
           return NULL;                                \
       __res;                                          \
    })

#define RETHROW_PN(e)        \
    ({ typeof(e) __res = (e);                          \
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

#define THROW_IF(e, val)                               \
    do {                                               \
        if (unlikely(e))                               \
            return (val);                              \
    } while (0)
#define THROW_UNLESS(e, val)    THROW_IF(!(e), (val))

#define THROW_NULL_IF(e)        THROW_IF((e), NULL)
#define THROW_NULL_UNLESS(e)    THROW_UNLESS((e), NULL)
#define THROW_ERR_IF(e)         THROW_IF((e), -1)
#define THROW_ERR_UNLESS(e)     THROW_UNLESS((e), -1)
#define THROW_FALSE_IF(e)       THROW_IF((e), false)
#define THROW_FALSE_UNLESS(e)   THROW_UNLESS((e), false)


#ifdef CMP
# error CMP already defined
#endif
#ifdef SIGN
# error SIGN already defined
#endif

enum sign {
    NEGATIVE = -1,
    ZERO     = 0,
    POSITIVE = 1,
};

#define CMP(x, y)     cast(enum sign, ((x) > (y)) - ((x) < (y)))
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
        } const *__p;                          \
        __p = cast(typeof(__p), ptr);          \
        __p->__v;                              \
    })
#define get_unaligned(ptr)  get_unaligned_type(typeof(*(ptr)), ptr)

#define put_unaligned_type(type_t, ptr, v)     \
    ({                                         \
        struct __attribute__((packed)) {       \
            type_t __v;                        \
        } *__p;                                \
        type_t __v = (v);                      \
        __p = cast(typeof(__p), ptr);          \
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

#define fieldsizeof(type_t, m)  sizeof(cast(type_t *, 0)->m)
#define fieldtypeof(type_t, m)  typeof(cast(type_t *, 0)->m)
#define countof(table)          (cast(ssize_t, sizeof(table) / sizeof((table)[0]) \
                                      + __must_be_array(table)))
#define ssizeof(foo)            (cast(ssize_t, sizeof(foo)))

#define bitsizeof(type_t)       (sizeof(type_t) * CHAR_BIT)
#define BITS_TO_ARRAY_LEN(type_t, nbits)  \
    DIV_ROUND_UP(nbits, bitsizeof(type_t))

#define BITMASK_NTH(type_t, n) ( cast(type_t, 1) << ((n) & (bitsizeof(type_t) - 1)))
#define BITMASK_LT(type_t, n)  (BITMASK_NTH(type_t, n) - 1)
#define BITMASK_LE(type_t, n)  ((BITMASK_NTH(type_t, n) << 1) - 1)
#define BITMASK_GE(type_t, n)  (~cast(type_t, 0) << ((n) & (bitsizeof(type_t) - 1)))
#define BITMASK_GT(type_t, n)  (BITMASK_GE(type_t, n) << 1)

#define OP_BIT(bits, n, shift, op) \
    ((bits)[(unsigned)(n) / (shift)] op BITMASK_NTH(typeof(*(bits)), n))
#define TST_BIT(bits, n)    OP_BIT(bits, n, bitsizeof(*(bits)), &  )
#define SET_BIT(bits, n)    (void)OP_BIT(bits, n, bitsizeof(*(bits)), |= )
#define RST_BIT(bits, n)    (void)OP_BIT(bits, n, bitsizeof(*(bits)), &=~)
#define CLR_BIT(bits, n)    RST_BIT(bits, n)
#define XOR_BIT(bits, n)    (void)OP_BIT(bits, n, bitsizeof(*(bits)), ^= )

#ifdef __GNUC__
# define container_of(obj, type_t, member) \
      ({ const typeof(cast(type_t *, 0)->member) *__mptr = (obj);              \
         cast(type_t *, cast(char *, __mptr) - offsetof(type_t, member)); })
#else
# define container_of(obj, type_t, member) \
      cast(type_t *, cast(char *, (obj)) - offsetof(type_t, member))
#endif


/*---------------- Loops -------------------------*/

/* Standard loops for structures of the form struct { type_t *tab; int len; }.
 */

#define tab_for_each_pos(pos, vec)                                           \
    for (int pos = 0; pos < (vec)->len; pos++)

#define tab_for_each_ptr(ptr, vec)                                           \
    for (__unused__ typeof(*(vec)->tab) *ptr = (vec)->tab,                   \
         *__i_##ptr = (vec)->tab;                                            \
         __i_##ptr < (vec)->tab + (vec)->len;                                \
         ptr = ++__i_##ptr)

#define tab_for_each_entry(e, vec)                                           \
    for (typeof(*(vec)->tab) e,                                              \
                            *e##__ptr = ({                                   \
                                if ((vec)->len) {                            \
                                    e = *(vec)->tab;                         \
                                } else {                                     \
                                    /* Avoid warnings with old gcc's */      \
                                    p_clear(&e, 1);                          \
                                }                                            \
                                (vec)->tab;                                  \
                            });                                              \
         ({                                                                  \
             bool e##__res = e##__ptr < (vec)->tab + (vec)->len;             \
             if (e##__res) {                                                 \
                 e = *(e##__ptr++);                                          \
             }                                                               \
             e##__res;                                                       \
         });)

#define tab_for_each_pos_rev(pos, vec)                                       \
    for (int pos = (vec)->len; pos-- > 0; )

#define tab_for_each_pos_safe(pos, vec)                                      \
    tab_for_each_pos_rev(pos, vec)

/* Standard loops for C arrays (ex: int values[] = { 1, 2, 3 }) */

#define carray_for_each_pos(pos, array)                                      \
    for (int pos = 0; pos < countof(array); pos++)

#define carray_for_each_ptr(ptr, array)                                      \
    for (__unused__ typeof(*array) *ptr = (array),                           \
         *__i_##ptr = (array);                                               \
         __i_##ptr < (array) + countof(array);                               \
         ptr = ++__i_##ptr)

#define carray_for_each_entry(e, array)                                      \
    for (typeof(*array) e, *e##__ptr = (array);                              \
         ({                                                                  \
              bool e##__res = e##__ptr < (array) + countof(array);           \
              if (e##__res) {                                                \
                  e = *(e##__ptr++);                                         \
              }                                                              \
              e##__res;                                                      \
         });)


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
