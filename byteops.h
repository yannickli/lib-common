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

#ifndef IS_LIB_COMMON_BYTEOPS_H
#define IS_LIB_COMMON_BYTEOPS_H

#include <endian.h>

#ifdef __LITTLE_ENDIAN
static inline uint16_t cpu_to_le_16(uint16_t x) {return x;}
static inline uint32_t cpu_to_le_32(uint32_t x) {return x;}
static inline uint64_t cpu_to_le_64(uint64_t x) {return x;}

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
#else
#error "not supported yet"
#endif

/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_BYTEOPS_H */
