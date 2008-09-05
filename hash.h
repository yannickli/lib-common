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

#ifndef IS_LIB_COMMON_HASH_H
#define IS_LIB_COMMON_HASH_H

#include "core.h"
#include "str-conv.h"

#include "hash-config.h"
#include "hash-bignum.h"
#include "hash-bn_mul.h"

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

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define GET_U32_LE(n,b,i)    memcpy(&(n), (b) + (i), 4)
#define PUT_U32_LE(n,b,i)    (*(uint32_t *)((b) + (i)) = (n))
#define GET_U32_BE(n,b,i)    (memcpy(&(n), (b) + (i), 4), (n) = bswap32(n))
#define PUT_U32_BE(n,b,i)    (*(uint32_t *)((b) + (i)) = bswap32(n))
#else
#define GET_U32_LE(n,b,i)    (memcpy(&(n), (b) + (i), 4), (n) = bswap32(n))
#define PUT_U32_LE(n,b,i)    (*(uint32_t *)((b) + (i)) = bswap32(n))
#define GET_U32_BE(n,b,i)    memcpy(&(n), (b) + (i), 4)
#define PUT_U32_BE(n,b,i)    (*(uint32_t *)((b) + (i)) = (n))
#endif

#include "hash-aes.h"
#include "hash-arc4.h"
#include "hash-config.h"
#include "hash-des.h"
#include "hash-havege.h"
#include "hash-md2.h"
#include "hash-md4.h"
#include "hash-md5.h"
#include "hash-padlock.h"
#include "hash-rsa.h"
#include "hash-sha1.h"
#include "hash-sha2.h"
#include "hash-sha4.h"

uint32_t icrc32(uint32_t crc, const void *data, ssize_t len);

uint32_t hsieh_hash(const void *s, int len);
uint32_t jenkins_hash(const void *s, int len);

#endif
