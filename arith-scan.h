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

#if !defined(IS_LIB_COMMON_ARITH_H) || defined(IS_LIB_COMMON_ARITH_SCAN_H)
#  error "you must include arith.h instead"
#else
#define IS_LIB_COMMON_ARITH_SCAN_H

/* This module implements optimized scans primitives on data. The scans are
 * optimized with SSE instruction sets, as a consequence, we requires the
 * memory to be aligned on 128bits
 */

bool is_memory_zero(const void *data, size_t len);

ssize_t scan_non_zero16(const uint16_t u16[], size_t pos, size_t len);
ssize_t scan_non_zero32(const uint32_t u32[], size_t pos, size_t len);

size_t count_non_zero8(const uint8_t u8[], size_t len);
size_t count_non_zero16(const uint16_t u16[], size_t len);
size_t count_non_zero32(const uint32_t u32[], size_t len);
extern size_t (*count_non_zero64)(const uint64_t u64[], size_t len);
size_t count_non_zero128(const void *u128, size_t len);

#endif
