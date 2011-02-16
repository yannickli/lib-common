/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_ARITH_H) || defined(IS_LIB_COMMON_ARITH_STR_STREAM_H)
#  error "you must include arith.h instead"
#else
#define IS_LIB_COMMON_ARITH_STR_STREAM_H

#include "str.h"

#define __PS_GET(ps, w, endianess)                                   \
    do {                                                             \
        endianess##w##_t res = get_unaligned_##endianess##w(ps->p);  \
        __ps_skip(ps, w / 8);                                        \
        return res;                                                  \
    } while (0)

#define PS_GET(ps, res, w, endianess)               \
    do {                                            \
        PS_WANT(ps_has(ps, w / 8));                 \
        *res = __ps_get_##endianess##w(ps);         \
        return 0;                                   \
    } while (0)

static inline uint16_t __ps_get_be16(pstream_t *ps) { __PS_GET(ps, 16, be); }
static inline uint32_t __ps_get_be24(pstream_t *ps) { __PS_GET(ps, 24, be); }
static inline uint32_t __ps_get_be32(pstream_t *ps) { __PS_GET(ps, 32, be); }
static inline uint64_t __ps_get_be48(pstream_t *ps) { __PS_GET(ps, 48, be); }
static inline uint64_t __ps_get_be64(pstream_t *ps) { __PS_GET(ps, 64, be); }

static inline uint16_t __ps_get_le16(pstream_t *ps) { __PS_GET(ps, 16, le); }
static inline uint32_t __ps_get_le24(pstream_t *ps) { __PS_GET(ps, 24, le); }
static inline uint32_t __ps_get_le32(pstream_t *ps) { __PS_GET(ps, 32, le); }
static inline uint64_t __ps_get_le48(pstream_t *ps) { __PS_GET(ps, 48, le); }
static inline uint64_t __ps_get_le64(pstream_t *ps) { __PS_GET(ps, 64, le); }

static inline uint16_t __ps_get_cpu16(pstream_t *ps) {
    uint16_t res = get_unaligned_cpu16(ps->p);
    __ps_skip(ps, 16 / 8);
    return res;
}
static inline uint32_t __ps_get_cpu32(pstream_t *ps) {
    uint32_t res = get_unaligned_cpu32(ps->p);
    __ps_skip(ps, 32 / 8);
    return res;
}
static inline uint64_t __ps_get_cpu64(pstream_t *ps) {
    uint64_t res = get_unaligned_cpu64(ps->p);
    __ps_skip(ps, 64 / 8);
    return res;
}

static inline float __ps_get_float_le(pstream_t *ps) {
    float res = get_unaligned_float_le(ps->p);
    __ps_skip(ps, sizeof(float));
    return res;
}
static inline int ps_get_float_le(pstream_t *ps, float *out) {
    PS_WANT(ps_has(ps, sizeof(float)));
    *out = __ps_get_float_le(ps);
    return 0;
}
static inline double __ps_get_double_le(pstream_t *ps) {
    double res = get_unaligned_double_le(ps->p);
    __ps_skip(ps, sizeof(double));
    return res;
}
static inline int ps_get_double_le(pstream_t *ps, double *out) {
    PS_WANT(ps_has(ps, sizeof(double)));
    *out = __ps_get_double_le(ps);
    return 0;
}

static inline float __ps_get_float_be(pstream_t *ps) {
    float res = get_unaligned_float_be(ps->p);
    __ps_skip(ps, sizeof(float));
    return res;
}
static inline int ps_get_float_be(pstream_t *ps, float *out) {
    PS_WANT(ps_has(ps, sizeof(float)));
    *out = __ps_get_float_be(ps);
    return 0;
}
static inline double __ps_get_double_be(pstream_t *ps) {
    double res = get_unaligned_double_be(ps->p);
    __ps_skip(ps, sizeof(double));
    return res;
}
static inline int ps_get_double_be(pstream_t *ps, double *out) {
    PS_WANT(ps_has(ps, sizeof(double)));
    *out = __ps_get_double_be(ps);
    return 0;
}

static inline int ps_get_be16(pstream_t *ps, uint16_t *res) { PS_GET(ps, res, 16, be); }
static inline int ps_get_be24(pstream_t *ps, uint32_t *res) { PS_GET(ps, res, 24, be); }
static inline int ps_get_be32(pstream_t *ps, uint32_t *res) { PS_GET(ps, res, 32, be); }
static inline int ps_get_be48(pstream_t *ps, uint64_t *res) { PS_GET(ps, res, 48, be); }
static inline int ps_get_be64(pstream_t *ps, uint64_t *res) { PS_GET(ps, res, 64, be); }

static inline int ps_get_le16(pstream_t *ps, uint16_t *res) { PS_GET(ps, res, 16, le); }
static inline int ps_get_le24(pstream_t *ps, uint32_t *res) { PS_GET(ps, res, 24, le); }
static inline int ps_get_le32(pstream_t *ps, uint32_t *res) { PS_GET(ps, res, 32, le); }
static inline int ps_get_le48(pstream_t *ps, uint64_t *res) { PS_GET(ps, res, 48, le); }
static inline int ps_get_le64(pstream_t *ps, uint64_t *res) { PS_GET(ps, res, 64, le); }

static inline int ps_get_cpu16(pstream_t *ps, uint16_t *res) { PS_GET(ps, res, 16, cpu); }
static inline int ps_get_cpu32(pstream_t *ps, uint32_t *res) { PS_GET(ps, res, 32, cpu); }
static inline int ps_get_cpu64(pstream_t *ps, uint64_t *res) { PS_GET(ps, res, 64, cpu); }

#undef __PS_GET
#undef PS_GET

#endif
