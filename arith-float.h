/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2016 INTERSEC SA                                   */
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
    union { float x; uint32_t u32; } u;
    u.x = x;
    return u.u32;
}
static inline uint64_t double_bits_(double x) {
    union { double x; uint64_t u64; } u;
    u.x = x;
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


static inline float bits_to_float_(uint32_t x) {
    union { float x; uint32_t u32; } u;
    u.u32 = x;
    return u.x;
}

static inline double bits_to_double_(uint64_t x) {
    union { double x; uint64_t u64; } u;
    u.u64 = x;
    return u.x;
}

static inline float bits_to_float_cpu(uint32_t x) {
#if __FLOAT_WORD_ORDER == __BYTE_ORDER
    return bits_to_float_(x);
#else
    return bits_to_float_(bswap32(x));
#endif
}
static inline double bits_to_double_cpu(uint64_t x) {
#if __FLOAT_WORD_ORDER == __BYTE_ORDER
    return bits_to_double_(x);
#else
    return bits_to_double_(bswap64(x));
#endif
}

static inline float bits_to_float_le(le32_t x) {
#if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
    return bits_to_float_(force_cast(uint32_t, x));
#else
    return bits_to_float_(bswap32(force_cast(uint32_t, x)));
#endif
}
static inline double bits_to_double_le(le64_t x) {
#if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
    return bits_to_double_(force_cast(uint64_t, x));
#else
    return bits_to_double_(bswap64(force_cast(uint64_t, x)));
#endif
}

static inline float bits_to_float_be(le32_t x) {
#if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
    return bits_to_float_(bswap32(force_cast(uint32_t, x)));
#else
    return bits_to_float_(force_cast(uint32_t, x));
#endif
}
static inline double bits_to_double_be(le64_t x) {
#if __FLOAT_WORD_ORDER == __LITTLE_ENDIAN
    return bits_to_double_(bswap64(force_cast(uint64_t, x)));
#else
    return bits_to_double_(force_cast(uint64_t, x));
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
    return bits_to_float_(get_unaligned_le32(p));
}
static inline double get_unaligned_double_le(const void *p) {
    return bits_to_double_(get_unaligned_le64(p));
}

static inline float get_unaligned_float_be(const void *p) {
    return bits_to_float_(get_unaligned_be32(p));
}
static inline double get_unaligned_double_be(const void *p) {
    return bits_to_double_(get_unaligned_be64(p));
}


/** Round a double value to a given precision.
 *
 * Round a double value to a given precision, expressed in number of decimal
 * digits.
 *
 * Examples:
 *  - double_round(12.1234567, 0) -> 12.0
 *  - double_round(12.6,       0) -> 13.0
 *  - double_round(12.1234567, 3) -> 12.123
 *  - double_round(12.1234567, 4) -> 12.1235
 *
 * \param[in]  val        the input value to round.
 * \param[in]  precision  the precision, in number of decimal digits.
 *
 * \return  the rounded value.
 */
double double_round(double val, uint8_t precision);

#endif
