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

#if !defined(IS_LIB_COMMON_ARITH_H) || defined(IS_LIB_COMMON_ARITH_ENDIANESS_H)
#  error "you must include <lib-common/arith.h> instead"
#else
#define IS_LIB_COMMON_ARITH_ENDIANESS_H

/*----- byte swapping stuff -----*/

#ifdef __GLIBC__
#include <byteswap.h>
static inline uint64_t bswap64(uint64_t x) { return bswap_64(x); }
static inline uint32_t bswap32(uint32_t x) { return bswap_32(x); }
static inline uint16_t bswap16(uint16_t x) { return bswap_16(x); }
#elif defined(__GNUC__) && \
    ((__GNUC__ << 16) + __GNUC_MINOR__) >= ((4 << 16) + 3)
static inline uint64_t bswap64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t bswap32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t bswap16(uint16_t x) { return (x >> 8) | (x << 8); }
}
#else
#  error please implement byte swap ops.
#endif

#define bswap16_const(x) \
    ((((uint16_t)(x)) << 8) | (((uint16_t)(x)) >> 8))
#define bswap32_const(x) \
    ((((uint32_t)(x) >> 24) & 0x000000ff) | \
     (((uint32_t)(x) >>  8) & 0x0000ff00) | \
     (((uint32_t)(x) <<  8) & 0x00ff0000) | \
     (((uint32_t)(x) << 24) & 0xff000000))
#define bswap64_const(x) \
    (((uint64_t)bswap32_const(x) << 32) | \
     (bswap32_const((uint64_t)(x) >> 32)))

#if __BYTE_ORDER == __BIG_ENDIAN
#  define ntohl_const(x)    force_cast(uint32_t, (x))
#  define ntohs_const(x)    force_cast(uint16_t, (x))
#  define htonl_const(x)    force_cast(be32_t, (x))
#  define htons_const(x)    force_cast(be16_t, (x))
#else
#  define ntohl_const(x)    force_cast(uint32_t, bswap32_const(x))
#  define ntohs_const(x)    force_cast(uint16_t, bswap16_const(x))
#  define htonl_const(x)    force_cast(be32_t, bswap32_const(x))
#  define htons_const(x)    force_cast(be16_t, bswap16_const(x))
#endif

#ifdef __SPARSE__
#include <arpa/inet.h>
#undef  htonl
#undef  htons
#undef  ntohl
#undef  ntohs
#define htonl htonl_const
#undef  htons htons_const
#undef  ntohl ntohl_const
#define ntohs ntohs_const
#endif


#if __BYTE_ORDER == __BIG_ENDIAN
#  define CPU_TO_LE(w, x)        force_cast(le##w##_t, bswap##w(x))
#  define CPU_TO_BE(w, x)        force_cast(be##w##_t, x)
#  define LE_TO_CPU(w, x)        force_cast(uint##w##_t, bswap##w(x))
#  define BE_TO_CPU(w, x)        force_cast(uint##w##_t, x)
#  define CPU_TO_LE_CONST(w, x)  force_cast(le##w##_t, bswap##w##_const(x))
#  define CPU_TO_BE_CONST(w, x)  force_cast(be##w##_t, x)
#else
#  define CPU_TO_LE(w, x)        force_cast(le##w##_t, bswap##w(x))
#  define CPU_TO_BE(w, x)        force_cast(be##w##_t, x)
#  define LE_TO_CPU(w, x)        force_cast(uint##w##_t, bswap##w(x))
#  define BE_TO_CPU(w, x)        force_cast(uint##w##_t, x)
#  define CPU_TO_LE_CONST(w, x)  force_cast(le##w##_t, bswap##w##_const(x))
#  define CPU_TO_BE_CONST(w, x)  force_cast(be##w##_t, x)
#endif

#define BE16_T(x)  CPU_TO_BE_CONST(16, x)
#define BE32_T(x)  CPU_TO_BE_CONST(32, x)
#define BE64_T(x)  CPU_TO_BE_CONST(64, x)

#define LE16_T(x)  CPU_TO_LE_CONST(16, x)
#define LE32_T(x)  CPU_TO_LE_CONST(32, x)
#define LE64_T(x)  CPU_TO_LE_CONST(64, x)

static inline uint16_t cpu16_get_unaligned(const void *p) {
    const struct __attribute__((packed)) { uint16_t x; } *s = p;
    return s->x;
}
static inline uint32_t cpu32_get_unaligned(const void *p) {
    const struct __attribute__((packed)) { uint32_t x; } *s = p;
    return s->x;
}
static inline uint64_t cpu64_get_unaligned(const void *p) {
    const struct __attribute__((packed)) { uint64_t x; } *s = p;
    return s->x;
}

static inline void cpu16_put_unaligned(void *p, uint16_t x) {
    struct __attribute__((packed)) { uint16_t x; } *s = p;
    s->x = x;
}
static inline void cpu32_put_unaligned(void *p, uint32_t x) {
    struct __attribute__((packed)) { uint32_t x; } *s = p;
    s->x = x;
}
static inline void cpu64_put_unaligned(void *p, uint64_t x) {
    struct __attribute__((packed)) { uint64_t x; } *s = p;
    s->x = x;
}

static inline le16_t cpu_to_le16(uint16_t x) { return CPU_TO_LE(16, x); }
static inline le32_t cpu_to_le32(uint32_t x) { return CPU_TO_LE(32, x); }
static inline le64_t cpu_to_le64(uint64_t x) { return CPU_TO_LE(64, x); }
static inline uint16_t le_to_cpu16(le16_t x) { return LE_TO_CPU(16, x); }
static inline uint32_t le_to_cpu32(le32_t x) { return LE_TO_CPU(32, x); }
static inline uint64_t le_to_cpu64(le64_t x) { return LE_TO_CPU(64, x); }

static inline le16_t cpu_to_le16p(const uint16_t *x) { return CPU_TO_LE(16, *x); }
static inline le32_t cpu_to_le32p(const uint32_t *x) { return CPU_TO_LE(32, *x); }
static inline le64_t cpu_to_le64p(const uint64_t *x) { return CPU_TO_LE(64, *x); }
static inline uint16_t le_to_cpu16p(const le16_t *x) { return LE_TO_CPU(16, *x); }
static inline uint32_t le_to_cpu32p(const le32_t *x) { return LE_TO_CPU(32, *x); }
static inline uint64_t le_to_cpu64p(const le64_t *x) { return LE_TO_CPU(64, *x); }

static inline le16_t cpu_to_le16pu(const void *x) {
    return CPU_TO_LE(16, cpu16_get_unaligned(x));
}
static inline le32_t cpu_to_le32pu(const void *x) {
    return CPU_TO_LE(32, cpu32_get_unaligned(x));
}
static inline le64_t cpu_to_le64pu(const void *x) {
    return CPU_TO_LE(64, cpu64_get_unaligned(x));
}
static inline uint16_t le_to_cpu16pu(const void *x) {
    return LE_TO_CPU(16, cpu16_get_unaligned(x));
}
static inline uint32_t le_to_cpu32pu(const void *x) {
    return LE_TO_CPU(32, cpu32_get_unaligned(x));
}
static inline uint64_t le_to_cpu64pu(const void *x) {
    return LE_TO_CPU(64, cpu64_get_unaligned(x));
}

static inline void put_unaligned_le16(void *p, uint16_t x) {
    cpu16_put_unaligned(p, CPU_TO_LE(16, x));
}
static inline void put_unaligned_le32(void *p, uint32_t x) {
    cpu32_put_unaligned(p, CPU_TO_LE(32, x));
}
static inline void put_unaligned_le64(void *p, uint64_t x) {
    cpu64_put_unaligned(p, CPU_TO_LE(64, x));
}

static inline be16_t cpu_to_be16(uint16_t x) { return CPU_TO_BE(16, x); }
static inline be32_t cpu_to_be32(uint32_t x) { return CPU_TO_BE(32, x); }
static inline be64_t cpu_to_be64(uint64_t x) { return CPU_TO_BE(64, x); }
static inline uint16_t be_to_cpu16(be16_t x) { return BE_TO_CPU(16, x); }
static inline uint32_t be_to_cpu32(be32_t x) { return BE_TO_CPU(32, x); }
static inline uint64_t be_to_cpu64(be64_t x) { return BE_TO_CPU(64, x); }

static inline be16_t cpu_to_be16p(const uint16_t *x) { return CPU_TO_BE(16, *x); }
static inline be32_t cpu_to_be32p(const uint32_t *x) { return CPU_TO_BE(32, *x); }
static inline be64_t cpu_to_be64p(const uint64_t *x) { return CPU_TO_BE(64, *x); }
static inline uint16_t be_to_cpu16p(const be16_t *x) { return BE_TO_CPU(16, *x); }
static inline uint32_t be_to_cpu32p(const be32_t *x) { return BE_TO_CPU(32, *x); }
static inline uint64_t be_to_cpu64p(const be64_t *x) { return BE_TO_CPU(64, *x); }

static inline be16_t cpu_to_be16pu(const void *x) {
    return CPU_TO_BE(16, cpu16_get_unaligned(x));
}
static inline be32_t cpu_to_be32pu(const void *x) {
    return CPU_TO_BE(32, cpu32_get_unaligned(x));
}
static inline be64_t cpu_to_be64pu(const void *x) {
    return CPU_TO_BE(64, cpu64_get_unaligned(x));
}
static inline uint16_t be_to_cpu16pu(const void *x) {
    return BE_TO_CPU(16, cpu16_get_unaligned(x));
}
static inline uint32_t be_to_cpu32pu(const void *x) {
    return BE_TO_CPU(32, cpu32_get_unaligned(x));
}
static inline uint64_t be_to_cpu64pu(const void *x) {
    return BE_TO_CPU(64, cpu64_get_unaligned(x));
}

static inline void put_unaligned_be16(void *p, uint16_t x) {
    cpu16_put_unaligned(p, CPU_TO_LE(16, x));
}
static inline void put_unaligned_be32(void *p, uint32_t x) {
    cpu32_put_unaligned(p, CPU_TO_LE(32, x));
}
static inline void put_unaligned_be64(void *p, uint64_t x) {
    cpu64_put_unaligned(p, CPU_TO_LE(64, x));
}

#endif
