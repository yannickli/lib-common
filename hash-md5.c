/*
 *  RFC 1321 compliant MD5 implementation
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
 *  The MD5 algorithm was designed by Ron Rivest in 1991.
 *
 *  http://www.ietf.org/rfc/rfc1321.txt
 */

#include "z.h"
#include "hash.h"

/*
 * MD5 context setup
 */
void md5_starts( md5_ctx *ctx )
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
}

static void md5_process( md5_ctx *ctx, const byte data[64] )
{
    uint32_t X[16], A, B, C, D;

    GET_U32_LE( X[ 0], data,  0 );
    GET_U32_LE( X[ 1], data,  4 );
    GET_U32_LE( X[ 2], data,  8 );
    GET_U32_LE( X[ 3], data, 12 );
    GET_U32_LE( X[ 4], data, 16 );
    GET_U32_LE( X[ 5], data, 20 );
    GET_U32_LE( X[ 6], data, 24 );
    GET_U32_LE( X[ 7], data, 28 );
    GET_U32_LE( X[ 8], data, 32 );
    GET_U32_LE( X[ 9], data, 36 );
    GET_U32_LE( X[10], data, 40 );
    GET_U32_LE( X[11], data, 44 );
    GET_U32_LE( X[12], data, 48 );
    GET_U32_LE( X[13], data, 52 );
    GET_U32_LE( X[14], data, 56 );
    GET_U32_LE( X[15], data, 60 );

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define P(a,b,c,d,k,s,t)                                \
{                                                       \
    a += F(b,c,d) + X[k] + t; a = S(a,s) + b;           \
}

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

#define F(x,y,z) (z ^ (x & (y ^ z)))

    P( A, B, C, D,  0,  7, 0xD76AA478 );
    P( D, A, B, C,  1, 12, 0xE8C7B756 );
    P( C, D, A, B,  2, 17, 0x242070DB );
    P( B, C, D, A,  3, 22, 0xC1BDCEEE );
    P( A, B, C, D,  4,  7, 0xF57C0FAF );
    P( D, A, B, C,  5, 12, 0x4787C62A );
    P( C, D, A, B,  6, 17, 0xA8304613 );
    P( B, C, D, A,  7, 22, 0xFD469501 );
    P( A, B, C, D,  8,  7, 0x698098D8 );
    P( D, A, B, C,  9, 12, 0x8B44F7AF );
    P( C, D, A, B, 10, 17, 0xFFFF5BB1 );
    P( B, C, D, A, 11, 22, 0x895CD7BE );
    P( A, B, C, D, 12,  7, 0x6B901122 );
    P( D, A, B, C, 13, 12, 0xFD987193 );
    P( C, D, A, B, 14, 17, 0xA679438E );
    P( B, C, D, A, 15, 22, 0x49B40821 );

#undef F

#define F(x,y,z) (y ^ (z & (x ^ y)))

    P( A, B, C, D,  1,  5, 0xF61E2562 );
    P( D, A, B, C,  6,  9, 0xC040B340 );
    P( C, D, A, B, 11, 14, 0x265E5A51 );
    P( B, C, D, A,  0, 20, 0xE9B6C7AA );
    P( A, B, C, D,  5,  5, 0xD62F105D );
    P( D, A, B, C, 10,  9, 0x02441453 );
    P( C, D, A, B, 15, 14, 0xD8A1E681 );
    P( B, C, D, A,  4, 20, 0xE7D3FBC8 );
    P( A, B, C, D,  9,  5, 0x21E1CDE6 );
    P( D, A, B, C, 14,  9, 0xC33707D6 );
    P( C, D, A, B,  3, 14, 0xF4D50D87 );
    P( B, C, D, A,  8, 20, 0x455A14ED );
    P( A, B, C, D, 13,  5, 0xA9E3E905 );
    P( D, A, B, C,  2,  9, 0xFCEFA3F8 );
    P( C, D, A, B,  7, 14, 0x676F02D9 );
    P( B, C, D, A, 12, 20, 0x8D2A4C8A );

#undef F

#define F(x,y,z) (x ^ y ^ z)

    P( A, B, C, D,  5,  4, 0xFFFA3942 );
    P( D, A, B, C,  8, 11, 0x8771F681 );
    P( C, D, A, B, 11, 16, 0x6D9D6122 );
    P( B, C, D, A, 14, 23, 0xFDE5380C );
    P( A, B, C, D,  1,  4, 0xA4BEEA44 );
    P( D, A, B, C,  4, 11, 0x4BDECFA9 );
    P( C, D, A, B,  7, 16, 0xF6BB4B60 );
    P( B, C, D, A, 10, 23, 0xBEBFBC70 );
    P( A, B, C, D, 13,  4, 0x289B7EC6 );
    P( D, A, B, C,  0, 11, 0xEAA127FA );
    P( C, D, A, B,  3, 16, 0xD4EF3085 );
    P( B, C, D, A,  6, 23, 0x04881D05 );
    P( A, B, C, D,  9,  4, 0xD9D4D039 );
    P( D, A, B, C, 12, 11, 0xE6DB99E5 );
    P( C, D, A, B, 15, 16, 0x1FA27CF8 );
    P( B, C, D, A,  2, 23, 0xC4AC5665 );

#undef F

#define F(x,y,z) (y ^ (x | ~z))

    P( A, B, C, D,  0,  6, 0xF4292244 );
    P( D, A, B, C,  7, 10, 0x432AFF97 );
    P( C, D, A, B, 14, 15, 0xAB9423A7 );
    P( B, C, D, A,  5, 21, 0xFC93A039 );
    P( A, B, C, D, 12,  6, 0x655B59C3 );
    P( D, A, B, C,  3, 10, 0x8F0CCC92 );
    P( C, D, A, B, 10, 15, 0xFFEFF47D );
    P( B, C, D, A,  1, 21, 0x85845DD1 );
    P( A, B, C, D,  8,  6, 0x6FA87E4F );
    P( D, A, B, C, 15, 10, 0xFE2CE6E0 );
    P( C, D, A, B,  6, 15, 0xA3014314 );
    P( B, C, D, A, 13, 21, 0x4E0811A1 );
    P( A, B, C, D,  4,  6, 0xF7537E82 );
    P( D, A, B, C, 11, 10, 0xBD3AF235 );
    P( C, D, A, B,  2, 15, 0x2AD7D2BB );
    P( B, C, D, A,  9, 21, 0xEB86D391 );

#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}

/*
 * MD5 process buffer
 */
void md5_update( md5_ctx *ctx, const void *_input, int ilen )
{
    const byte *input = _input;
    int fill;
    uint32_t left;

    if( ilen <= 0 )
        return;

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if( ctx->total[0] < (uint32_t) ilen )
        ctx->total[1]++;

    if( left && ilen >= fill )
    {
        memcpy( (void *) (ctx->buffer + left),
                (void *) input, fill );
        md5_process( ctx, ctx->buffer );
        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while( ilen >= 64 )
    {
        md5_process( ctx, input );
        input += 64;
        ilen  -= 64;
    }

    if( ilen > 0 )
    {
        memcpy( (void *) (ctx->buffer + left),
                (void *) input, ilen );
    }
}

static const byte md5_padding[64] =
{
 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * MD5 final digest
 */
void md5_finish( md5_ctx *ctx, byte output[16] )
{
    uint32_t last, padn;
    uint32_t high, low;
    byte msglen[8];

    high = ( ctx->total[0] >> 29 )
         | ( ctx->total[1] <<  3 );
    low  = ( ctx->total[0] <<  3 );

    PUT_U32_LE( low,  msglen, 0 );
    PUT_U32_LE( high, msglen, 4 );

    last = ctx->total[0] & 0x3F;
    padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

    md5_update( ctx, (byte *) md5_padding, padn );
    md5_update( ctx, msglen, 8 );

    PUT_U32_LE( ctx->state[0], output,  0 );
    PUT_U32_LE( ctx->state[1], output,  4 );
    PUT_U32_LE( ctx->state[2], output,  8 );
    PUT_U32_LE( ctx->state[3], output, 12 );
}

void md5_finish_hex( md5_ctx *ctx, char output[33] )
{
    byte digest[16];
    md5_finish(ctx, digest);
    strconv_hexencode(output, 33, digest, MD5_DIGEST_SIZE);
}

/*
 * output = MD5( input buffer )
 */
void md5( const void *input, int ilen, byte output[16] )
{
    md5_ctx ctx;

    md5_starts( &ctx );
    md5_update( &ctx, input, ilen );
    md5_finish( &ctx, output );

    memset( &ctx, 0, sizeof( md5_ctx ) );
}

uint64_t md5_hash_64(const void *data, int len)
{
    union {
        byte b[16];
        uint64_t u[2];
    } res;

    md5(data, len, res.b);
    return res.u[0] ^ res.u[1];
}

/*
 * output = MD5( input buffer )
 */
void md5_hex( const void *input, int ilen, char output[33] )
{
    md5_ctx ctx;

    md5_starts( &ctx );
    md5_update( &ctx, input, ilen );
    md5_finish_hex( &ctx, output );

    memset( &ctx, 0, sizeof( md5_ctx ) );
}

/*
 * output = MD5( file contents )
 */
int md5_file( char *path, byte output[16] )
{
    FILE *f;
    size_t n;
    md5_ctx ctx;
    byte buf[1024];

    if( ( f = fopen( path, "rb" ) ) == NULL )
        return( 1 );

    md5_starts( &ctx );

    while( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
        md5_update( &ctx, buf, (int) n );

    md5_finish( &ctx, output );

    memset( &ctx, 0, sizeof( md5_ctx ) );

    if( ferror( f ) != 0 )
    {
        fclose( f );
        return( 2 );
    }

    fclose( f );
    return( 0 );
}

/*
 * MD5 HMAC context setup
 */
void md5_hmac_starts( md5_ctx *ctx, const void *_key, int keylen )
{
    const byte *key = _key;
    int i;
    byte sum[16];

    if( keylen > 64 )
    {
        md5( key, keylen, sum );
        keylen = 16;
        key = sum;
    }

    memset( ctx->ipad, 0x36, 64 );
    memset( ctx->opad, 0x5C, 64 );

    for( i = 0; i < keylen; i++ )
    {
        ctx->ipad[i] = (byte)( ctx->ipad[i] ^ key[i] );
        ctx->opad[i] = (byte)( ctx->opad[i] ^ key[i] );
    }

    md5_starts( ctx );
    md5_update( ctx, ctx->ipad, 64 );

    memset( sum, 0, sizeof( sum ) );
}

/*
 * MD5 HMAC process buffer
 */
void md5_hmac_update( md5_ctx *ctx, const void *input, int ilen )
{
    md5_update( ctx, input, ilen );
}

/*
 * MD5 HMAC final digest
 */
void md5_hmac_finish( md5_ctx *ctx, byte output[16] )
{
    byte tmpbuf[16];

    md5_finish( ctx, tmpbuf );
    md5_starts( ctx );
    md5_update( ctx, ctx->opad, 64 );
    md5_update( ctx, tmpbuf, 16 );
    md5_finish( ctx, output );

    memset( tmpbuf, 0, sizeof( tmpbuf ) );
}

/*
 * output = HMAC-MD5( hmac key, input buffer )
 */
void md5_hmac( const void *key, int keylen, const void *input, int ilen,
               byte output[16] )
{
    md5_ctx ctx;

    md5_hmac_starts( &ctx, key, keylen );
    md5_hmac_update( &ctx, input, ilen );
    md5_hmac_finish( &ctx, output );

    memset( &ctx, 0, sizeof( md5_ctx ) );
}

/*
 * RFC 1321 test vectors
 */
static byte md5_test_buf[7][81] =
{
    { "" },
    { "a" },
    { "abc" },
    { "message digest" },
    { "abcdefghijklmnopqrstuvwxyz" },
    { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" },
    { "12345678901234567890123456789012345678901234567890123456789012" \
      "345678901234567890" }
};

static const int md5_test_buflen[7] =
{
    0, 1, 3, 14, 26, 62, 80
};

static const byte md5_test_sum[7][16] =
{
    { 0xD4, 0x1D, 0x8C, 0xD9, 0x8F, 0x00, 0xB2, 0x04,
      0xE9, 0x80, 0x09, 0x98, 0xEC, 0xF8, 0x42, 0x7E },
    { 0x0C, 0xC1, 0x75, 0xB9, 0xC0, 0xF1, 0xB6, 0xA8,
      0x31, 0xC3, 0x99, 0xE2, 0x69, 0x77, 0x26, 0x61 },
    { 0x90, 0x01, 0x50, 0x98, 0x3C, 0xD2, 0x4F, 0xB0,
      0xD6, 0x96, 0x3F, 0x7D, 0x28, 0xE1, 0x7F, 0x72 },
    { 0xF9, 0x6B, 0x69, 0x7D, 0x7C, 0xB7, 0x93, 0x8D,
      0x52, 0x5A, 0x2F, 0x31, 0xAA, 0xF1, 0x61, 0xD0 },
    { 0xC3, 0xFC, 0xD3, 0xD7, 0x61, 0x92, 0xE4, 0x00,
      0x7D, 0xFB, 0x49, 0x6C, 0xCA, 0x67, 0xE1, 0x3B },
    { 0xD1, 0x74, 0xAB, 0x98, 0xD2, 0x77, 0xD9, 0xF5,
      0xA5, 0x61, 0x1C, 0x2C, 0x9F, 0x41, 0x9D, 0x9F },
    { 0x57, 0xED, 0xF4, 0xA2, 0x2B, 0xE3, 0xC9, 0x55,
      0xAC, 0x49, 0xDA, 0x2E, 0x21, 0x07, 0xB6, 0x7A }
};

/*
 * RFC 2202 test vectors
 */
static byte md5_hmac_test_key[7][26] =
{
    { "\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B\x0B" },
    { "Jefe" },
    { "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA" },
    { "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10"
      "\x11\x12\x13\x14\x15\x16\x17\x18\x19" },
    { "\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C\x0C" },
    { "" }, /* 0xAA 80 times */
    { "" }
};

static const int md5_hmac_test_keylen[7] =
{
    16, 4, 16, 25, 16, 80, 80
};

static byte md5_hmac_test_buf[7][74] =
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
    { "Test Using Larger Than Block-Size Key and Larger"
      " Than One Block-Size Data" }
};

static const int md5_hmac_test_buflen[7] =
{
    8, 28, 50, 50, 20, 54, 73
};

static const byte md5_hmac_test_sum[7][16] =
{
    { 0x92, 0x94, 0x72, 0x7A, 0x36, 0x38, 0xBB, 0x1C,
      0x13, 0xF4, 0x8E, 0xF8, 0x15, 0x8B, 0xFC, 0x9D },
    { 0x75, 0x0C, 0x78, 0x3E, 0x6A, 0xB0, 0xB5, 0x03,
      0xEA, 0xA8, 0x6E, 0x31, 0x0A, 0x5D, 0xB7, 0x38 },
    { 0x56, 0xBE, 0x34, 0x52, 0x1D, 0x14, 0x4C, 0x88,
      0xDB, 0xB8, 0xC7, 0x33, 0xF0, 0xE8, 0xB3, 0xF6 },
    { 0x69, 0x7E, 0xAF, 0x0A, 0xCA, 0x3A, 0x3A, 0xEA,
      0x3A, 0x75, 0x16, 0x47, 0x46, 0xFF, 0xAA, 0x79 },
    { 0x56, 0x46, 0x1E, 0xF2, 0x34, 0x2E, 0xDC, 0x00,
      0xF9, 0xBA, 0xB9, 0x95 },
    { 0x6B, 0x1A, 0xB7, 0xFE, 0x4B, 0xD7, 0xBF, 0x8F,
      0x0B, 0x62, 0xE6, 0xCE, 0x61, 0xB9, 0xD0, 0xCD },
    { 0x6F, 0x63, 0x0F, 0xAD, 0x67, 0xCD, 0xA0, 0xEE,
      0x1F, 0xB1, 0xF5, 0x62, 0xDB, 0x3A, 0xA5, 0x3E }
};

/*
 * Checkup routine
 */
Z_GROUP_EXPORT(md5)
{
    byte buf[1024];
    byte md5sum[16];
    md5_ctx ctx;

    Z_TEST(hash, "") {
        for (int i = 0; i < 7; i++) {
            md5(md5_test_buf[i], md5_test_buflen[i], md5sum);
            Z_ASSERT_EQUAL(md5sum, 16, md5_test_sum[i], 16);
        }
    } Z_TEST_END;

    Z_TEST(hmac, "") {
        for (int i = 0; i < 7; i++) {
            if (i == 5 || i == 6) {
                memset(buf, '\xAA', 80);
                md5_hmac_starts(&ctx, buf, 80);
            } else {
                md5_hmac_starts(&ctx, md5_hmac_test_key[i],
                                md5_hmac_test_keylen[i]);
            }

            md5_hmac_update(&ctx, md5_hmac_test_buf[i],
                            md5_hmac_test_buflen[i]);
            md5_hmac_finish(&ctx, md5sum);

            if (i == 4) {
                Z_ASSERT_EQUAL(md5sum, 12, md5_hmac_test_sum[i], 12);
            } else {
                Z_ASSERT_EQUAL(md5sum, 16, md5_hmac_test_sum[i], 16);
            }
        }
    } Z_TEST_END;
} Z_GROUP_END
