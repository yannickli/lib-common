/*
 *  FIPS-180-2 compliant SHA-256 implementation
 *
 *  Copyright (C) 2006-2007  Christophe Devine
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of XySSL nor the names of its contributors may be
 *      used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *  The SHA-256 Secure Hash Standard was published by NIST in 2002.
 *
 *  http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

#include "z.h"
#include "hash.h"

#define ATTRS
#define F(x)  x

#include "hash-sha2.in.c"

#undef F
#undef ATTRS

void sha2_finish_hex( sha2_ctx *ctx, char output[65] )
{
    byte digest[32];
    sha2_finish(ctx, digest);
    strconv_hexencode(output, 65, digest,
                      ctx->is224 ? SHA224_DIGEST_SIZE : SHA256_DIGEST_SIZE);
}

/*
 * output = SHA-256( input buffer )
 */
void sha2_hex( const void *input, ssize_t ilen, char output[65], int is224 )
{
    sha2_ctx ctx;

    sha2_starts( &ctx, is224 );
    sha2_update( &ctx, input, ilen );
    sha2_finish_hex( &ctx, output );

    memset( &ctx, 0, sizeof( sha2_ctx ) );
}

/*
 * output = SHA-256( file contents )
 */
int sha2_file(const char *path, byte output[32], int is224 )
{
    FILE *f;
    size_t n;
    sha2_ctx ctx;
    byte buf[1024];

    if( ( f = fopen( path, "rb" ) ) == NULL )
        return( 1 );

    sha2_starts( &ctx, is224 );

    while( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
        sha2_update( &ctx, buf, (int) n );

    sha2_finish( &ctx, output );

    memset( &ctx, 0, sizeof( sha2_ctx ) );

    if( ferror( f ) != 0 )
    {
        fclose( f );
        return( 2 );
    }

    fclose( f );
    return( 0 );
}

/*
 * output = HMAC-SHA-256( hmac key, input buffer )
 */
void sha2_hmac( const void *key, int keylen,
                const void *input, ssize_t ilen,
                byte output[32], int is224 )
{
    sha2_ctx ctx;

    sha2_hmac_starts( &ctx, key, keylen, is224 );
    sha2_hmac_update( &ctx, input, ilen );
    sha2_hmac_finish( &ctx, output );

    memset( &ctx, 0, sizeof( sha2_ctx ) );
}

/* {{{ SHA-256 Crypt */

/* Based on Ulrich Drepper's Unix crypt with SHA256, version 0.4 2008-4-3,
 * released by Ulrich Drepper in public domain */

/* Table used for base64 transformation */
static const char sha2_crypt_base64char_g[64] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void sha2_crypt(const void *input, size_t ilen, const void *salt,
                size_t slen, uint32_t rounds, sb_t *output)
{
    uint32_t i;
    unsigned char alt_result[32];
    unsigned char tmp_result[32];
    sha2_ctx ctx;
    sha2_ctx alt_ctx;
    SB_1k(buf);
    SB_1k(alt_buf);

    slen = MIN(slen, SHA256_CRYPT_SALT_LEN_MAX);
    rounds = (rounds > 0) ? rounds : SHA256_CRYPT_DEFAULT_ROUNDS;
    rounds = MAX(rounds, SHA256_CRYPT_MIN_ROUNDS);
    rounds = MIN(rounds, SHA256_CRYPT_MAX_ROUNDS);

    /* Let's begin */
    sha2_starts(&ctx, false);

    /* Add the input string */
    sha2_update(&ctx, input, ilen);

    /* Add the salt */
    sha2_update(&ctx, salt, slen);

    /* Alternate hash with INPUT-SALT-INPUT */
    sha2_starts(&alt_ctx, false);
    sha2_update(&alt_ctx, input, ilen);
    sha2_update(&alt_ctx, salt, slen);
    sha2_update(&alt_ctx, input, ilen);
    sha2_finish(&alt_ctx, alt_result);

    for (i = ilen; i > 32; i -= 32) {
        sha2_update(&ctx, alt_result, 32);
    }
    sha2_update(&ctx, alt_result, i);

    /* For every 1 in the binary representation of ilen add alt,
     * for every 0 add the input
     */
    for (i = ilen; i > 0; i >>= 1) {
        if ((i & 1) != 0) {
            sha2_update(&ctx, alt_result, 32);
        } else {
            sha2_update(&ctx, input, ilen);
        }
    }

    /* Intermediate result */
    sha2_finish(&ctx, alt_result);

    /* New alternate hash */
    sha2_starts(&alt_ctx, false);

    /* For every character in the input add the entire input */
    for (i = 0; i < ilen; i++) {
        sha2_update(&alt_ctx, input, ilen);
    }

    /* Get temp result */
    sha2_finish(&alt_ctx, tmp_result);

    for (i = ilen; i >= 32; i -= 32) {
        sb_add(&buf, tmp_result, 32);
    }
    sb_add(&buf, tmp_result, i);

    sha2_starts(&alt_ctx, false);

    /* For each character in the input, add input */
    for (i = 0; i < 16U + alt_result[0]; i++) {
        sha2_update(&alt_ctx, salt, slen);
    }

    sha2_finish(&alt_ctx, tmp_result);

    for (i = slen; i >= 32; i -= 32) {
        sb_add(&alt_buf, tmp_result, 32);
    }
    sb_add(&alt_buf, tmp_result, i);

    /* The loop */
    for (i = 0; i < rounds; i++) {
        sha2_starts(&ctx, false);

        /* Add input or last result */
        if ((i & 1) != 0) {
            sha2_update(&ctx, buf.data, buf.len);
        } else {
            sha2_update(&ctx, alt_result, 32);
        }

        /* Add salt for numbers not divisible by 3 */
        if (i % 3 != 0) {
            sha2_update(&ctx, alt_buf.data, alt_buf.len);
        }

        /* Add input for numbers not divisible by 7 */
        if (i % 7 != 0) {
            sha2_update(&ctx, buf.data, buf.len);
        }

        /* Add intput or last result */
        if ((i & 1) != 0) {
            sha2_update(&ctx, alt_result, 32);
        } else {
            sha2_update(&ctx, buf.data, buf.len);
        }

        /* Create intermediate result */
        sha2_finish(&ctx, alt_result);
    }

    /* Construction of the result */
    sb_reset(output);
    sb_addf(output, SHA256_PREFIX "$");

    /* Here, we ALWAYS put the round prefix and round number in the result
     * string, not only if round number != default number.
     * This seems very less dangerous for compatibility between
     * encryption version used in our products. It is still compliant with
     * the specifications, even if the implementation given as example by
     * Drepper does not follow this precept.
     */
    sb_addf(output, SHA256_ROUNDS_PREFIX);
    sb_addf(output, "%d$", rounds);
    sb_add(output, salt, slen);
    sb_addf(output, "$");

#define b64_from_24bits(b2_, b1_, b0_, n_)                                  \
    do {                                                                    \
        unsigned int w = cpu_to_le32(((b2_) << 16) | ((b1_) << 8) | (b0_)); \
        for (int i_ = n_; i_-- > 0; w >>= 6) {                              \
            sb_add(output, &sha2_crypt_base64char_g[w & 0x3f], 1);          \
        }                                                                   \
    } while (0)

    b64_from_24bits(alt_result[0],   alt_result[10],  alt_result[20], 4);
    b64_from_24bits(alt_result[21],  alt_result[1],   alt_result[11], 4);
    b64_from_24bits(alt_result[12],  alt_result[22],  alt_result[2],  4);
    b64_from_24bits(alt_result[3],   alt_result[13],  alt_result[23], 4);
    b64_from_24bits(alt_result[24],  alt_result[4],   alt_result[14], 4);
    b64_from_24bits(alt_result[15],  alt_result[25],  alt_result[5],  4);
    b64_from_24bits(alt_result[6],   alt_result[16],  alt_result[26], 4);
    b64_from_24bits(alt_result[27],  alt_result[7],   alt_result[17], 4);
    b64_from_24bits(alt_result[18],  alt_result[28],  alt_result[8],  4);
    b64_from_24bits(alt_result[9],   alt_result[19],  alt_result[29], 4);
    b64_from_24bits(0,               alt_result[31],  alt_result[30], 3);

#undef b64_from_24bits

    /* Clearing datas to avoid core dump attacks */
    sha2_starts(&ctx, false);
    sha2_finish(&ctx, alt_result);

    p_clear(&tmp_result, 1);
    p_clear(&ctx, 1);
    p_clear(&alt_ctx, 1);
}

int sha2_crypt_parse(lstr_t input, int *out_rounds, pstream_t *out_salt,
                     pstream_t *out_hash)
{
    /* Correct format for SHA256-crypt is
     * $5$rounds=2000$salt$hash */

    pstream_t ps = ps_initlstr(&input);
    pstream_t salt;
    pstream_t hash;
    int rounds;
    static bool first_call = true;
    static ctype_desc_t sha2_crypt_ctype;

    out_salt = out_salt ?: &salt;
    out_hash = out_hash ?: &hash;
    out_rounds = out_rounds ?: &rounds;

    /* Check prefix */
    RETHROW(ps_skipstr(&ps, "$5$rounds="));

    /* Check rounds */
    errno = 0;
    *out_rounds = ps_geti(&ps);
    THROW_ERR_IF(errno != 0);
    THROW_ERR_IF(*out_rounds < SHA256_CRYPT_MIN_ROUNDS);
    THROW_ERR_IF(*out_rounds > SHA256_CRYPT_MAX_ROUNDS);

    /* Check salt */
    RETHROW(ps_skipc(&ps, '$'));
    RETHROW(ps_get_ps_chr_and_skip(&ps, '$', out_salt));
    THROW_ERR_IF(ps_done(out_salt));
    THROW_ERR_IF(ps_len(out_salt) > SHA256_CRYPT_SALT_LEN_MAX);

    /* Check hash-part */
    if (unlikely(first_call)) {
        first_call = false;
        ctype_desc_build2(&sha2_crypt_ctype, sha2_crypt_base64char_g,
                          sizeof(sha2_crypt_base64char_g));
    }
    THROW_ERR_IF(ps_len(&ps) != SHA256_CRYPT_DIGEST_SIZE);
    *out_hash = ps;
    ps_skip_span(&ps, &sha2_crypt_ctype);
    THROW_ERR_IF(!ps_done(&ps));

    return 0;
}

/* }}} */

/*
 * FIPS-180-2 test vectors
 */
static byte sha2_test_buf[3][57] =
{
    { "abc" },
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" },
    { "" }
};

static const int sha2_test_buflen[3] =
{
    3, 56, 1000
};

static const byte sha2_test_sum[6][32] =
{
    /*
     * SHA-224 test vectors
     */
    { 0x23, 0x09, 0x7D, 0x22, 0x34, 0x05, 0xD8, 0x22,
      0x86, 0x42, 0xA4, 0x77, 0xBD, 0xA2, 0x55, 0xB3,
      0x2A, 0xAD, 0xBC, 0xE4, 0xBD, 0xA0, 0xB3, 0xF7,
      0xE3, 0x6C, 0x9D, 0xA7 },
    { 0x75, 0x38, 0x8B, 0x16, 0x51, 0x27, 0x76, 0xCC,
      0x5D, 0xBA, 0x5D, 0xA1, 0xFD, 0x89, 0x01, 0x50,
      0xB0, 0xC6, 0x45, 0x5C, 0xB4, 0xF5, 0x8B, 0x19,
      0x52, 0x52, 0x25, 0x25 },
    { 0x20, 0x79, 0x46, 0x55, 0x98, 0x0C, 0x91, 0xD8,
      0xBB, 0xB4, 0xC1, 0xEA, 0x97, 0x61, 0x8A, 0x4B,
      0xF0, 0x3F, 0x42, 0x58, 0x19, 0x48, 0xB2, 0xEE,
      0x4E, 0xE7, 0xAD, 0x67 },

    /*
     * SHA-256 test vectors
     */
    { 0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
      0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
      0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
      0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD },
    { 0x24, 0x8D, 0x6A, 0x61, 0xD2, 0x06, 0x38, 0xB8,
      0xE5, 0xC0, 0x26, 0x93, 0x0C, 0x3E, 0x60, 0x39,
      0xA3, 0x3C, 0xE4, 0x59, 0x64, 0xFF, 0x21, 0x67,
      0xF6, 0xEC, 0xED, 0xD4, 0x19, 0xDB, 0x06, 0xC1 },
    { 0xCD, 0xC7, 0x6E, 0x5C, 0x99, 0x14, 0xFB, 0x92,
      0x81, 0xA1, 0xC7, 0xE2, 0x84, 0xD7, 0x3E, 0x67,
      0xF1, 0x80, 0x9A, 0x48, 0xA4, 0x97, 0x20, 0x0E,
      0x04, 0x6D, 0x39, 0xCC, 0xC7, 0x11, 0x2C, 0xD0 }
};

/*
 * RFC 4231 test vectors
 */
static byte sha2_hmac_test_key[7][26] =
{
    { "\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B"
      "\x0B\x0B\x0B\x0B" },
    { "Jefe" },
    { "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
      "\xAA\xAA\xAA\xAA" },
    { "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
      "\x11\x12\x13\x14\x15\x16\x17\x18\x19" },
    { "\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C"
      "\x0C\x0C\x0C\x0C" },
    { "" }, /* 0xAA 131 times */
    { "" }
};

static const int sha2_hmac_test_keylen[7] =
{
    20, 4, 20, 25, 20, 131, 131
};

static byte sha2_hmac_test_buf[7][153] =
{
    { "Hi There" },
    { "what do ya want for nothing?" },
    { "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
      "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
      "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
      "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD"
      "\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD\xDD" },
    { "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
      "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
      "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
      "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
      "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD" },
    { "Test With Truncation" },
    { "Test Using Larger Than Block-Size Key - Hash Key First" },
    { "This is a test using a larger than block-size key "
      "and a larger than block-size data. The key needs to "
      "be hashed before being used by the HMAC algorithm." }
};

static const int sha2_hmac_test_buflen[7] =
{
    8, 28, 50, 50, 20, 54, 152
};

static const byte sha2_hmac_test_sum[14][32] =
{
    /*
     * HMAC-SHA-224 test vectors
     */
    { 0x89, 0x6F, 0xB1, 0x12, 0x8A, 0xBB, 0xDF, 0x19,
      0x68, 0x32, 0x10, 0x7C, 0xD4, 0x9D, 0xF3, 0x3F,
      0x47, 0xB4, 0xB1, 0x16, 0x99, 0x12, 0xBA, 0x4F,
      0x53, 0x68, 0x4B, 0x22 },
    { 0xA3, 0x0E, 0x01, 0x09, 0x8B, 0xC6, 0xDB, 0xBF,
      0x45, 0x69, 0x0F, 0x3A, 0x7E, 0x9E, 0x6D, 0x0F,
      0x8B, 0xBE, 0xA2, 0xA3, 0x9E, 0x61, 0x48, 0x00,
      0x8F, 0xD0, 0x5E, 0x44 },
    { 0x7F, 0xB3, 0xCB, 0x35, 0x88, 0xC6, 0xC1, 0xF6,
      0xFF, 0xA9, 0x69, 0x4D, 0x7D, 0x6A, 0xD2, 0x64,
      0x93, 0x65, 0xB0, 0xC1, 0xF6, 0x5D, 0x69, 0xD1,
      0xEC, 0x83, 0x33, 0xEA },
    { 0x6C, 0x11, 0x50, 0x68, 0x74, 0x01, 0x3C, 0xAC,
      0x6A, 0x2A, 0xBC, 0x1B, 0xB3, 0x82, 0x62, 0x7C,
      0xEC, 0x6A, 0x90, 0xD8, 0x6E, 0xFC, 0x01, 0x2D,
      0xE7, 0xAF, 0xEC, 0x5A },
    { 0x0E, 0x2A, 0xEA, 0x68, 0xA9, 0x0C, 0x8D, 0x37,
      0xC9, 0x88, 0xBC, 0xDB, 0x9F, 0xCA, 0x6F, 0xA8 },
    { 0x95, 0xE9, 0xA0, 0xDB, 0x96, 0x20, 0x95, 0xAD,
      0xAE, 0xBE, 0x9B, 0x2D, 0x6F, 0x0D, 0xBC, 0xE2,
      0xD4, 0x99, 0xF1, 0x12, 0xF2, 0xD2, 0xB7, 0x27,
      0x3F, 0xA6, 0x87, 0x0E },
    { 0x3A, 0x85, 0x41, 0x66, 0xAC, 0x5D, 0x9F, 0x02,
      0x3F, 0x54, 0xD5, 0x17, 0xD0, 0xB3, 0x9D, 0xBD,
      0x94, 0x67, 0x70, 0xDB, 0x9C, 0x2B, 0x95, 0xC9,
      0xF6, 0xF5, 0x65, 0xD1 },

    /*
     * HMAC-SHA-256 test vectors
     */
    { 0xB0, 0x34, 0x4C, 0x61, 0xD8, 0xDB, 0x38, 0x53,
      0x5C, 0xA8, 0xAF, 0xCE, 0xAF, 0x0B, 0xF1, 0x2B,
      0x88, 0x1D, 0xC2, 0x00, 0xC9, 0x83, 0x3D, 0xA7,
      0x26, 0xE9, 0x37, 0x6C, 0x2E, 0x32, 0xCF, 0xF7 },
    { 0x5B, 0xDC, 0xC1, 0x46, 0xBF, 0x60, 0x75, 0x4E,
      0x6A, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xC7,
      0x5A, 0x00, 0x3F, 0x08, 0x9D, 0x27, 0x39, 0x83,
      0x9D, 0xEC, 0x58, 0xB9, 0x64, 0xEC, 0x38, 0x43 },
    { 0x77, 0x3E, 0xA9, 0x1E, 0x36, 0x80, 0x0E, 0x46,
      0x85, 0x4D, 0xB8, 0xEB, 0xD0, 0x91, 0x81, 0xA7,
      0x29, 0x59, 0x09, 0x8B, 0x3E, 0xF8, 0xC1, 0x22,
      0xD9, 0x63, 0x55, 0x14, 0xCE, 0xD5, 0x65, 0xFE },
    { 0x82, 0x55, 0x8A, 0x38, 0x9A, 0x44, 0x3C, 0x0E,
      0xA4, 0xCC, 0x81, 0x98, 0x99, 0xF2, 0x08, 0x3A,
      0x85, 0xF0, 0xFA, 0xA3, 0xE5, 0x78, 0xF8, 0x07,
      0x7A, 0x2E, 0x3F, 0xF4, 0x67, 0x29, 0x66, 0x5B },
    { 0xA3, 0xB6, 0x16, 0x74, 0x73, 0x10, 0x0E, 0xE0,
      0x6E, 0x0C, 0x79, 0x6C, 0x29, 0x55, 0x55, 0x2B },
    { 0x60, 0xE4, 0x31, 0x59, 0x1E, 0xE0, 0xB6, 0x7F,
      0x0D, 0x8A, 0x26, 0xAA, 0xCB, 0xF5, 0xB7, 0x7F,
      0x8E, 0x0B, 0xC6, 0x21, 0x37, 0x28, 0xC5, 0x14,
      0x05, 0x46, 0x04, 0x0F, 0x0E, 0xE3, 0x7F, 0x54 },
    { 0x9B, 0x09, 0xFF, 0xA7, 0x1B, 0x94, 0x2F, 0xCB,
      0x27, 0x63, 0x5F, 0xBC, 0xD5, 0xB0, 0xE9, 0x44,
      0xBF, 0xDC, 0x63, 0x64, 0x4F, 0x07, 0x13, 0x93,
      0x8A, 0x7F, 0x51, 0x53, 0x5C, 0x3A, 0x35, 0xE2 }
};

/*
 * Checkup routine
 */
Z_GROUP_EXPORT(sha2)
{
    byte buf[1024];
    byte sha2sum[32];
    sha2_ctx ctx;
    int len;

    Z_TEST(hash, "") {
        for (int i = 0; i < 6; i++) {
            int j = i % 3;
            int k = i < 3;

            sha2_starts( &ctx, k );

            if (j == 2) {
                memset(buf, 'a', 1000);
                for (int l = 0; l < 1000; l++)
                    sha2_update(&ctx, buf, 1000);
            } else {
                sha2_update(&ctx, sha2_test_buf[j], sha2_test_buflen[j]);
            }

            sha2_finish(&ctx, sha2sum);

            if (j == 4) {
                len = 16;
            } else {
                len = 32 - k * 4;
            }
            Z_ASSERT_EQUAL(sha2sum, len, sha2_test_sum[i], len);
        }
    } Z_TEST_END;

    Z_TEST(hmac, "") {
        for (int i = 0; i < 14; i++) {
            int j = i % 7;
            int k = i < 7;

            if (j == 5 || j == 6) {
                memset( buf, '\xAA', 131);
                sha2_hmac_starts(&ctx, buf, 131, k);
            } else {
                sha2_hmac_starts(&ctx, sha2_hmac_test_key[j],
                                 sha2_hmac_test_keylen[j], k);
            }
            sha2_hmac_update(&ctx, sha2_hmac_test_buf[j],
                                    sha2_hmac_test_buflen[j]);

            sha2_hmac_finish(&ctx, sha2sum);
            if (j == 4) {
                len = 16;
            } else {
                len = 32 - k * 4;
            }
            Z_ASSERT_EQUAL(sha2sum, len, sha2_hmac_test_sum[i], len);
        }
    } Z_TEST_END;

    Z_TEST(crypt, "") {

        /* Those are extracted from Ulrich Drepper's
         * "Unix crypt using SHA-256" specifications v0.4 2008-4-3
         */
        static const struct {
            const char *input;
            const int   rounds;
            const char *salt;
            const int   expected_rounds;
            const char *expected_salt;
            const char *expected_hash;
        } tests_crypt[] =
        {
            {"Hello world!", 10000, "saltstringsaltstring", 10000,
             "saltstringsaltst",
             "3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA"},
            {"This is just a test", 5000, "toolongsaltstring",
             5000, "toolongsaltstrin",
             "Un/5jzAHMgOGZ5.mWJpuVolil07guHPvOW8mGRcvxa5"},
            {"a very much longer text to encrypt.  This one even stretche"
             "s over morethan one line.", 1400, "anotherlongsaltstring",
             1400, "anotherlongsalts",
             "Rx.j8H.h8HjEDGomFU8bDkXm3XIUnzyxf12oP84Bnq1"},
            {"we have a short salt string but not a short password", 77777,
             "short", 77777, "short",
             "JiO1O3ZpDAxGJeaDIuqCoEFysAe1mZNJRs3pw0KQRd/"},
            {"a short string", 123456, "asaltof16chars..",
             123456, "asaltof16chars..",
             "gP3VQ/6X7UUEW3HkBn2w1/Ptq2jxPyzV/cZKmF/wJvD"},
            {"the minimum number is still observed", 10, "roundstoolow",
             1000, "roundstoolow",
             "yfvwcWrQ8l/K0DAWyuPMDNHpIVlTQebY9l/gL972bIC"},
        };

        SB_1k(crypt_buf);

        for (int i = 0; i < countof(tests_crypt); ++i) {
            t_scope;
            lstr_t expected;
            int rounds;
            pstream_t salt;
            pstream_t hash;

            sha2_crypt(tests_crypt[i].input, strlen(tests_crypt[i].input),
                       tests_crypt[i].salt, strlen(tests_crypt[i].salt),
                       tests_crypt[i].rounds, &crypt_buf);

            expected = t_lstr_fmt("$5$rounds=%d$%s$%s",
                                  tests_crypt[i].expected_rounds,
                                  tests_crypt[i].expected_salt,
                                  tests_crypt[i].expected_hash);
            Z_ASSERT_LSTREQUAL(LSTR_SB_V(&crypt_buf),
                               expected);

            Z_ASSERT_N(sha2_crypt_parse(LSTR_SB_V(&crypt_buf),
                                             &rounds, &salt, &hash));
            Z_ASSERT_EQ(rounds, tests_crypt[i].expected_rounds);
            Z_ASSERT_LSTREQUAL(LSTR_PS_V(&salt),
                               LSTR(tests_crypt[i].expected_salt));
            Z_ASSERT_LSTREQUAL(LSTR_PS_V(&hash),
                               LSTR(tests_crypt[i].expected_hash));

        }

        /* Check the parse function on bad cases */

#define CHECK_FAIL(str)                \
    Z_ASSERT_NEG(sha2_crypt_parse(LSTR(str), NULL, NULL, NULL))

        /* Wrong algorithm code */
        CHECK_FAIL("$6$rounds=10000$salt"
                   "$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA");
        /* Wrong rounds format */
        CHECK_FAIL("$5$rounds=a10000$salt"
                   "$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA");
        /* Rounds count too low */
        CHECK_FAIL("$5$rounds=999$salt"
                   "$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA");
        /* Rounds count too high */
        CHECK_FAIL("$5$rounds=1000000000$salt"
                   "$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA");
        /* Wrong rounds format */
        CHECK_FAIL("$5$rounds=10000a$salt"
                   "$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA");
        /* No separator between salt and hash */
        CHECK_FAIL("$5$rounds=10000$salt"
                   "3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA");
        /* Empty salt */
        CHECK_FAIL("$5$rounds=10000$"
                   "$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA");
        /* Salt string too long */
        CHECK_FAIL("$5$rounds=10000$toolongsaltstring"
                   "$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA");
        /* Bad character in hash */
        CHECK_FAIL("$5$rounds=10000$salt"
                   "$3xv.=bSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcA");
        /* Hash too long */
        CHECK_FAIL("$5$rounds=10000$salt"
                   "$3xv.VbSHBb41AL9AvLeujZkZRBAwqFMz2.opqey6IcAa");

#undef CHECK_FAIL

    } Z_TEST_END;
} Z_GROUP_END
