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

#ifndef IS_LIB_COMMON_HASH_H
#define IS_LIB_COMMON_HASH_H

#include "core.h"
#include "arith.h"

#define SHA1_DIGEST_SIZE    (160 / 8)
#define SHA224_DIGEST_SIZE  (224 / 8)
#define SHA256_DIGEST_SIZE  (256 / 8)
#define SHA384_DIGEST_SIZE  (384 / 8)
#define SHA512_DIGEST_SIZE  (512 / 8)
#define MD5_DIGEST_SIZE     (128 / 8)

#define MD5_HEX_DIGEST_SIZE     (MD5_DIGEST_SIZE    * 2 + 1)
#define SHA1_HEX_DIGEST_SIZE    (SHA1_DIGEST_SIZE   * 2 + 1)
#define SHA224_HEX_DIGEST_SIZE  (SHA224_DIGEST_SIZE * 2 + 1)
#define SHA256_HEX_DIGEST_SIZE  (SHA256_DIGEST_SIZE * 2 + 1)
#define SHA384_HEX_DIGEST_SIZE  (SHA384_DIGEST_SIZE * 2 + 1)
#define SHA512_HEX_DIGEST_SIZE  (SHA512_DIGEST_SIZE * 2 + 1)

#define SHA1_BLOCK_SIZE     64
#define SHA256_BLOCK_SIZE   ( 512 / 8)
#define SHA512_BLOCK_SIZE   (1024 / 8)
#define SHA384_BLOCK_SIZE   SHA512_BLOCK_SIZE
#define SHA224_BLOCK_SIZE   SHA256_BLOCK_SIZE

#define GET_U32_LE(n,b,i)    ((n) = cpu_to_le32pu((b) + (i)))
#define PUT_U32_LE(n,b,i)    (*acast(le32_t, (b) + (i)) = cpu_to_le32(n))
#define GET_U32_BE(n,b,i)    ((n) = cpu_to_be32pu((b) + (i)))
#define PUT_U32_BE(n,b,i)    (*acast(be32_t, (b) + (i)) = cpu_to_be32(n))

#include "hash-aes.h"
#include "hash-arc4.h"
#include "hash-des.h"
#include "hash-md2.h"
#include "hash-md4.h"
#include "hash-md5.h"
#include "hash-padlock.h"
#include "hash-sha1.h"
#include "hash-sha2.h"
#include "hash-sha4.h"

#include "hash-iop.h"

uint32_t icrc32(uint32_t crc, const void *data, ssize_t len) __leaf;
uint64_t icrc64(uint64_t crc, const void *data, ssize_t len) __leaf;

uint32_t hsieh_hash(const void *s, ssize_t len) __leaf;
uint32_t jenkins_hash(const void *s, ssize_t len) __leaf;

#ifdef __cplusplus
#define murmur_128bits_buf char out[]
#else
#define murmur_128bits_buf char out[static 16]
#endif

uint32_t murmur_hash3_x86_32 (const void *key, size_t len, uint32_t seed)
    __leaf;
void     murmur_hash3_x86_128(const void *key, size_t len, uint32_t seed,
                              murmur_128bits_buf) __leaf;
void     murmur_hash3_x64_128(const void *key, size_t len, uint32_t seed,
                              murmur_128bits_buf) __leaf;

static inline uint32_t mem_hash32(const void *data, ssize_t len)
{
    if (unlikely(len < 0))
        len = strlen((const char *)data);
#if defined(__x86_64__) || defined(__i386__)
    return murmur_hash3_x86_32(data, len, 0xdeadc0de);
#else
    return jenkins_hash(data, len);
#endif
}

static inline uint32_t u64_hash32(uint64_t u64)
{
    return (uint32_t)(u64) ^ (uint32_t)(u64 >> 32);
}

#endif
