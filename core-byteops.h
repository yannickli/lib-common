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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_BYTEOPS_H)
#  error "you must include <lib-common/core.h> instead"
#endif
#define IS_LIB_COMMON_CORE_BYTEOPS_H

typedef uint64_t __bitwise__ be64_t;
typedef uint64_t __bitwise__ le64_t;
typedef uint32_t __bitwise__ le32_t;
typedef uint32_t __bitwise__ be32_t;
typedef uint16_t __bitwise__ le16_t;
typedef uint16_t __bitwise__ be16_t;

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

#ifdef __SPARSE__
#include <arpa/inet.h>
#undef htonl
#undef htons
#undef ntohl
#undef ntohs
static inline be32_t htonl(uint32_t x) {
    return force_cast(be32_t, x);
}
static inline be16_t htons(uint16_t x) {
    return force_cast(be16_t, x);
}
static inline uint32_t ntohl(be32_t x) {
    return force_cast(uint32_t, x);
}
static inline uint16_t ntohs(be16_t x) {
    return force_cast(uint16_t, x);
}
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
#  define ntohl_const(x)    (x)
#  define ntohs_const(x)    (x)
#  define htonl_const(x)    (x)
#  define htons_const(x)    (x)
#else
#  define ntohl_const(x) \
        ((((x) >> 24) & 0x000000ff) | \
         (((x) >>  8) & 0x0000ff00) | \
         (((x) <<  8) & 0x00ff0000) | \
         (((x) << 24) & 0xff000000))
#  define ntohs_const(x) \
        ((((x) >> 8) & 0x00ff) | (((x) << 8) & 0xff00))
#  define htonl_const(x)    ntohl_const(x)
#  define htons_const(x)    ntohs_const(x)
#endif

#define BE32_T(x)  force_cast(be32_t, htonl_const(x))
#define BE16_T(x)  force_cast(be16_t, htons_const(x))

#define MAKE64(hi, lo)  (((uint64_t)(uint32_t)(hi) << 32) | (uint32_t)(lo))

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint16_t cpu_to_le_16(uint16_t x) { return x; }
static inline uint32_t cpu_to_le_32(uint32_t x) { return x; }
static inline uint64_t cpu_to_le_64(uint64_t x) { return x; }
static inline uint16_t cpu_to_be_16(uint16_t x) { return bswap16(x); }
static inline uint32_t cpu_to_be_32(uint32_t x) { return bswap32(x); }
static inline uint64_t cpu_to_be_64(uint64_t x) { return bswap64(x); }
#else
static inline uint16_t cpu_to_le_16(uint16_t x) { return bswap16(x); }
static inline uint32_t cpu_to_le_32(uint32_t x) { return bswap32(x); }
static inline uint64_t cpu_to_le_64(uint64_t x) { return bswap64(x); }
static inline uint16_t cpu_to_be_16(uint16_t x) { return x; }
static inline uint32_t cpu_to_be_32(uint32_t x) { return x; }
static inline uint64_t cpu_to_be_64(uint64_t x) { return x; }
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint16_t le_to_cpu_16(const byte *p) {
    return ((uint16_t)p[0] << 0) + ((uint16_t)p[1] << 8);
}
static inline uint32_t le_to_cpu_32(const byte *p) {
    return ((uint32_t)p[0] << 0) + ((uint32_t)p[1] << 8)
          +((uint32_t)p[2] << 16) + ((uint32_t)p[3] << 24);
}
static inline uint64_t le_to_cpu_64(const byte *p) {
    return ((uint64_t)p[0] << 0) + ((uint64_t)p[1] << 8)
          +((uint64_t)p[2] << 16) + ((uint64_t)p[3] << 24)
          +((uint64_t)p[4] << 32) + ((uint64_t)p[5] << 40)
          +((uint64_t)p[6] << 48) + ((uint64_t)p[7] << 56);
}
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint16_t le_to_cpu_16(const byte *p) {
    return ((uint16_t)p[1] << 0) + ((uint16_t)p[0] << 8);
}
static inline uint32_t le_to_cpu_32(const byte *p) {
    return ((uint32_t)p[3] <<  0) + ((uint32_t)p[3] <<  8)
          +((uint32_t)p[1] << 16) + ((uint32_t)p[0] << 24);
}
static inline uint64_t le_to_cpu_64(const byte *p) {
    return ((uint64_t)p[7] <<  0) + ((uint64_t)p[6] <<  8)
          +((uint64_t)p[5] << 16) + ((uint64_t)p[4] << 24)
          +((uint64_t)p[3] << 32) + ((uint64_t)p[2] << 40)
          +((uint64_t)p[1] << 48) + ((uint64_t)p[0] << 56);
}
#endif


