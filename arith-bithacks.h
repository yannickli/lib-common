/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2009 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_ARITH_H) || defined(IS_LIB_COMMON_ARITH_BIHACKS_H)
#  error "you must include <lib-common/arith.h> instead"
#else
#define IS_LIB_COMMON_ARITH_BIHACKS_H

#if (SIZE_MAX == UINT32_MAX)
#  define I_WORDSIZE        32
#  define I_FASTBITCALL(f)  f##32
#elif (SIZE_MAX == UINT64_MAX)
#  define I_WORDSIZE        64
#  define I_FASTBITCALL(f)  f##64
#else
#  error  unable to detect your architecture wordsize
#endif

/* XXX bit scan reverse, only defined for u != 0
 * bsr32(0x1f) == 4 because first bit set from the "left" is 2^4
 */
static inline size_t bsr32(uint32_t u) { return __builtin_clz(u) ^ 31; }
static inline size_t bsr64(uint64_t u) { return __builtin_clzll(u) ^ 63; }
static inline size_t bsrsz(size_t   u) { return I_FASTBITCALL(bsr)(u); }

/* XXX bit scan forward, only defined for u != 0
 * bsf32(0xf10) == 4 because first bit set from the "right" is 2^4
 */
static inline size_t bsf32(uint32_t u) { return __builtin_ctz(u); }
static inline size_t bsf64(uint64_t u) { return __builtin_ctzll(u); }
static inline size_t bsfsz(size_t   u) { return I_FASTBITCALL(bsf)(u); }


/*----- bitcount -----*/

extern uint8_t const __bitcount11[1 << 11];

static inline uint8_t bitcount8(uint8_t n) {
    return __bitcount11[n];
}
static inline uint8_t bitcount16(size_t n) {
    return bitcount8(n) + bitcount8(n >> 8);
}

static inline uint8_t bitcount32(size_t n) {
    return __bitcount11[(n >>  0) & 0x7ff]
        +  __bitcount11[(n >> 11) & 0x7ff]
        +  __bitcount11[(n >> 22) & 0x7ff];
}
static inline uint8_t bitcount64(uint64_t n) {
    return __bitcount11[(n >>  0) & 0x7ff]
        +  __bitcount11[(n >> 11) & 0x7ff]
        +  __bitcount11[(n >> 22) & 0x7ff]
        +  __bitcount11[(n >> 33) & 0x7ff]
        +  __bitcount11[(n >> 44) & 0x7ff]
        +  __bitcount11[(n >> 55)];
}

static inline uint8_t bitcountsz(size_t n) {
    return I_FASTBITCALL(bitcount)(n);
}

size_t membitcount(const void *ptr, size_t n);

#endif
