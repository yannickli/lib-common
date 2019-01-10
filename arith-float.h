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

#if !defined(IS_LIB_COMMON_ARITH_H) || defined(IS_LIB_COMMON_ARITH_FLOAT_H)
#  error "you must include arith.h instead"
#else
#define IS_LIB_COMMON_ARITH_FLOAT_H

/*
 * Float:
 * - bit    31: sign bit
 * - bit 30-23: exponent
 * - bit 22- 0: fraction
 * bias: +127
 *
 * double:
 * - bit    63: sign bit
 * - bit 62-52: exponent
 * - bit 51- 0: fraction
 * bias: +1023
 *
 */
static inline void arith_float_assumptions(void) {
    STATIC_ASSERT(sizeof(float)  == sizeof(uint32_t));
    STATIC_ASSERT(sizeof(double) == sizeof(uint64_t));
}

static inline uint32_t float_bits_(float x) {
    union { float x; uint32_t u32; } u = { .x = x };
    return u.u32;
}
static inline uint64_t double_bits_(double x) {
    union { double x; uint64_t u64; } u = { .x = x };
    return u.u64;
}

static inline bool float_is_identical(float x, float y) {
    return float_bits_(x) == float_bits_(y);
}
static inline bool double_is_identical(double x, double y) {
    return double_bits_(x) == double_bits_(y);
}

static inline uint32_t float_bits_cpu(float x) {
#if __FLOAT_WORD_ORDER == __BYTE_ORDER
    return force_cast(uint32_t, float_bits_(x));
#else
    return force_cast(uint32_t, bswap32(float_bits_(x)));
#endif
}
static inline uint64_t double_bits_cpu(double x) {
#if __FLOAT_WORD_ORDER == __BYTE_ORDER
    return force_cast(uint64_t, double_bits_(x));
#else
    return force_cast(uint64_t, bswap64(double_bits_(x)));
#endif
}

static inline le32_t float_bits_le(float x) {
#if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
    return force_cast(le32_t, float_bits_(x));
#else
    return force_cast(le32_t, bswap32(float_bits_(x)));
#endif
}
static inline le64_t double_bits_le(double x) {
#if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
    return force_cast(le64_t, double_bits_(x));
#else
    return force_cast(le64_t, bswap64(double_bits_(x)));
#endif
}

static inline be32_t float_bits_be(float x) {
#if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
    return force_cast(be32_t, bswap32(float_bits_(x)));
#else
    return force_cast(be32_t, float_bits_(x));
#endif
}
static inline be64_t double_bits_be(double x) {
#if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
    return force_cast(be64_t, bswap64(double_bits_(x)));
#else
    return force_cast(be64_t, double_bits_(x));
#endif
}

static inline void *put_unaligned_float_le(void *p, float x) {
    return put_unaligned(p, float_bits_le(x));
}
static inline void *put_unaligned_double_le(void *p, double x) {
    return put_unaligned(p, double_bits_le(x));
}

static inline void *put_unaligned_float_be(void *p, float x) {
    return put_unaligned(p, float_bits_be(x));
}
static inline void *put_unaligned_double_be(void *p, double x) {
    return put_unaligned(p, double_bits_be(x));
}

static inline float get_unaligned_float_le(const void *p) {
    union { uint32_t u32; float d; } u;
    u.u32 = get_unaligned_cpu32(p);
#if __FLOAT_WORD_ORDER != __LITTLE_ENDIAN
    u.u32 = bswap32(u.u32);
#endif
    return u.d;
}
static inline double get_unaligned_double_le(const void *p) {
    union { uint64_t u64; double d; } u;
    u.u64 = get_unaligned_cpu64(p);
#if __FLOAT_WORD_ORDER != __LITTLE_ENDIAN
    u.u64 = bswap64(u.u64);
#endif
    return u.d;
}

static inline float get_unaligned_float_be(const void *p) {
    union { uint32_t u32; float d; } u;
    u.u32 = get_unaligned_cpu32(p);
#if __FLOAT_WORD_ORDER != __BIG_ENDIAN
    u.u32 = bswap32(u.u32);
#endif
    return u.d;
}
static inline double get_unaligned_double_be(const void *p) {
    union { uint64_t u64; double d; } u;
    u.u64 = get_unaligned_cpu64(p);
#if __FLOAT_WORD_ORDER != __BIG_ENDIAN
    u.u64 = bswap64(u.u64);
#endif
    return u.d;
}

#endif
