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

#if !defined(IS_LIB_COMMON_ARITH_H) || defined(IS_LIB_COMMON_ARITH_STR_STREAM_H)
#  error "you must include <lib-common/arith.h> instead"
#else
#define IS_LIB_COMMON_ARITH_STR_STREAM_H

#include "str.h"

#define __PS_GET(ps, w, endianess)                               \
    do {                                                         \
        endianess##w##_t res = cpu_to_##endianess##w##pu(ps->p); \
        __ps_skip(ps, w / 8);                                    \
        return res;                                              \
    } while (0)

#define PS_GET(ps, res, w, endianess)               \
    do {                                            \
        PS_WANT(ps_has(ps, w / 8));                 \
        *res = __ps_get_##endianess##w(ps);         \
        return 0;                                   \
    } while (0)

static inline be16_t __ps_get_be16(pstream_t *ps) { __PS_GET(ps, 16, be); }
static inline be32_t __ps_get_be32(pstream_t *ps) { __PS_GET(ps, 32, be); }
static inline be64_t __ps_get_be64(pstream_t *ps) { __PS_GET(ps, 64, be); }

static inline le16_t __ps_get_le16(pstream_t *ps) { __PS_GET(ps, 16, le); }
static inline le32_t __ps_get_le32(pstream_t *ps) { __PS_GET(ps, 32, le); }
static inline le64_t __ps_get_le64(pstream_t *ps) { __PS_GET(ps, 64, le); }

static inline uint16_t __ps_get_cpu16(pstream_t *ps) {
    uint16_t res = cpu16_get_unaligned(ps->p);
    __ps_skip(ps, 16 / 8);
    return res;
}
static inline uint32_t __ps_get_cpu32(pstream_t *ps) {
    uint32_t res = cpu32_get_unaligned(ps->p);
    __ps_skip(ps, 32 / 8);
    return res;
}
static inline uint64_t __ps_get_cpu64(pstream_t *ps) {
    uint64_t res = cpu64_get_unaligned(ps->p);
    __ps_skip(ps, 64 / 8);
    return res;
}

static inline int ps_get_be16(pstream_t *ps, be16_t *res) { PS_GET(ps, res, 16, be); }
static inline int ps_get_be32(pstream_t *ps, be32_t *res) { PS_GET(ps, res, 32, be); }
static inline int ps_get_be64(pstream_t *ps, be64_t *res) { PS_GET(ps, res, 64, be); }

static inline int ps_get_le16(pstream_t *ps, le16_t *res) { PS_GET(ps, res, 16, le); }
static inline int ps_get_le32(pstream_t *ps, le32_t *res) { PS_GET(ps, res, 32, le); }
static inline int ps_get_le64(pstream_t *ps, le64_t *res) { PS_GET(ps, res, 64, le); }

static inline int ps_get_cpu16(pstream_t *ps, uint16_t *res) { PS_GET(ps, res, 16, cpu); }
static inline int ps_get_cpu32(pstream_t *ps, uint32_t *res) { PS_GET(ps, res, 32, cpu); }
static inline int ps_get_cpu64(pstream_t *ps, uint64_t *res) { PS_GET(ps, res, 64, cpu); }

#undef __PS_GET
#undef PS_GET

#endif
