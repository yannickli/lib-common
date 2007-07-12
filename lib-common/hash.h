/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
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

#include "macros.h"

uint32_t crc32(const void *data, ssize_t len);

/* BSD Code taken from http://www.ouah.org/ogay/sha2/sha2.tar.gz */

/*-
 * FIPS 180-2 SHA-224/256/384/512 implementation
 * Last update: 05/23/2005
 * Issue date:  04/30/2005
 *
 * Copyright (C) 2005 Olivier Gay <olivier.gay@a3.epfl.ch>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */



#define SHA1_DIGEST_SIZE    (160 / 8)
#define SHA224_DIGEST_SIZE  (224 / 8)
#define SHA256_DIGEST_SIZE  (256 / 8)
#define SHA384_DIGEST_SIZE  (384 / 8)
#define SHA512_DIGEST_SIZE  (512 / 8)

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

typedef struct sha1_ctx {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} sha1_ctx;

typedef struct sha256_ctx {
    uint32_t tot_len;
    uint32_t len;
    uint32_t h[8];
    byte block[2 * SHA256_BLOCK_SIZE];
} sha256_ctx;

typedef struct sha512_ctx {
    uint32_t tot_len;
    uint32_t len;
    uint64_t h[8];
    byte block[2 * SHA512_BLOCK_SIZE];
} sha512_ctx;

typedef sha512_ctx sha384_ctx;
typedef sha256_ctx sha224_ctx;

void sha1_init(sha1_ctx *ctx);
void sha1_update(sha1_ctx *ctx, const void *data, uint32_t len);
void sha1_final(sha1_ctx *ctx, byte *digest);
void sha1_final_hex(sha1_ctx *ctx, char *digest);
void sha1(const void *message, uint32_t len, byte *digest);
void sha1_hex(const void *message, uint32_t len, char *digest);

void sha224_init(sha224_ctx *ctx);
void sha224_update(sha224_ctx *ctx, const void *message, uint32_t len);
void sha224_final(sha224_ctx *ctx, byte *digest);
void sha224_final_hex(sha224_ctx *ctx, char *digest);
void sha224(const void *message, uint32_t len, byte *digest);
void sha224_hex(const void *message, uint32_t len, char *digest);

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const void *message, uint32_t len);
void sha256_final(sha256_ctx *ctx, byte *digest);
void sha256_final_hex(sha256_ctx *ctx, char *digest);
void sha256(const void *message, uint32_t len, byte *digest);
void sha256_hex(const void *message, uint32_t len, char *digest);

void sha384_init(sha384_ctx *ctx);
void sha384_update(sha384_ctx *ctx, const void *message, uint32_t len);
void sha384_final(sha384_ctx *ctx, byte *digest);
void sha384_final_hex(sha384_ctx *ctx, char *digest);
void sha384(const void *message, uint32_t len, byte *digest);
void sha384_hex(const void *message, uint32_t len, char *digest);

void sha512_init(sha512_ctx *ctx);
void sha512_update(sha512_ctx *ctx, const void *message, uint32_t len);
void sha512_final(sha512_ctx *ctx, byte *digest);
void sha512_final_hex(sha512_ctx *ctx, char *digest);
void sha512(const void *message, uint32_t len, byte *digest);
void sha512_hex(const void *message, uint32_t len, char *digest);

int sha_sum_file(const char *filename, char *digest, int shatype, int binary);

#endif /* IS_LIB_COMMON_HASH_H */
