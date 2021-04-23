/**
 * \file sha2.h
 */
#ifndef XYSSL_SHA2_H
#define XYSSL_SHA2_H

#define SHA256_PREFIX  "$5"
#define SHA256_ROUNDS_PREFIX  "rounds="

/**
 * \brief          SHA-256 context structure
 */
typedef struct {
    uint32_t total[2]; /*!< number of bytes processed  */
    uint32_t state[8]; /*!< intermediate digest state  */
    byte buffer[64];   /*!< data block being processed */

    byte ipad[64];     /*!< HMAC: inner padding        */
    byte opad[64];     /*!< HMAC: outer padding        */
    int is224;         /*!< 0 => SHA-256, else SHA-224 */
} sha2_ctx;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          SHA-256 context setup
 *
 * \param ctx      context to be initialized
 * \param is224    0 = use SHA256, 1 = use SHA224
 */
void sha2_starts(sha2_ctx *ctx, int is224) __leaf;

/**
 * \brief          SHA-256 process buffer
 *
 * \param ctx      SHA-256 context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void sha2_update(sha2_ctx *ctx, const void *input, ssize_t ilen) __leaf;

/**
 * \brief          SHA-256 final digest
 *
 * \param ctx      SHA-256 context
 * \param output   SHA-224/256 checksum result
 */
void sha2_finish(sha2_ctx *ctx, byte output[32]) __leaf;

/**
 * \brief          SHA-256 final digest
 *
 * \param ctx      SHA-256 context
 * \param output   SHA-224/256 checksum result
 */
void sha2_finish_hex(sha2_ctx *ctx, char output[65]) __leaf;

/**
 * \brief          Output = SHA-256(input buffer)
 *
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   SHA-224/256 checksum result
 * \param is224    0 = use SHA256, 1 = use SHA224
 */
void sha2(const void *input, ssize_t ilen,
          byte output[32], int is224) __leaf;

/**
 * \brief          Output = SHA-256(input buffer)
 *
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   SHA-224/256 checksum result
 * \param is224    0 = use SHA256, 1 = use SHA224
 */
void sha2_hex(const void *input, ssize_t ilen, char output[65], int is224)
    __leaf;

/**
 * \brief          Output = SHA-256(file contents)
 *
 * \param path     input file name
 * \param output   SHA-224/256 checksum result
 * \param is224    0 = use SHA256, 1 = use SHA224
 *
 * \return         0 if successful, 1 if fopen failed,
 *                 or 2 if fread failed
 */
int sha2_file(const char *path, byte output[32], int is224) __leaf;

/**
 * \brief          SHA-256 HMAC context setup
 *
 * \param ctx      HMAC context to be initialized
 * \param key      HMAC secret key
 * \param keylen   length of the HMAC key
 * \param is224    0 = use SHA256, 1 = use SHA224
 */
void sha2_hmac_starts(sha2_ctx *ctx, const void *key, int keylen,
                      int is224) __leaf;

/**
 * \brief          SHA-256 HMAC process buffer
 *
 * \param ctx      HMAC context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void sha2_hmac_update(sha2_ctx *ctx, const void *input, ssize_t ilen)
    __leaf;

/**
 * \brief          SHA-256 HMAC final digest
 *
 * \param ctx      HMAC context
 * \param output   SHA-224/256 HMAC checksum result
 */
void sha2_hmac_finish(sha2_ctx *ctx, byte output[32])
    __leaf;

/**
 * \brief          Output = HMAC-SHA-256(hmac key, input buffer)
 *
 * \param key      HMAC secret key
 * \param keylen   length of the HMAC key
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   HMAC-SHA-224/256 result
 * \param is224    0 = use SHA256, 1 = use SHA224
 */
void sha2_hmac(const void *key, int keylen, const void *input, ssize_t ilen,
               byte output[32], int is224) __leaf;

#define SHA256_CRYPT_SALT_LEN_MAX    16
#define SHA256_CRYPT_DEFAULT_ROUNDS  5000
#define SHA256_CRYPT_MIN_ROUNDS      1000
#define SHA256_CRYPT_MAX_ROUNDS      999999999
#define SHA256_CRYPT_DIGEST_SIZE     43

/**
 * \brief          Implementation of Ulrich Drepper's SHA256-Crypt
 *
 * \param intput   buffer holding the data
 * \param ilen     length of the input data
 * \param salt     buffer holding the salt string
 * \param slen     length of the salt string
 *                 (should be <= SHA256_CRYPT_SALT_LEN_MAX)
 * \param rounds   number of iteration. SHA256_CRYPT_DEFAULT_ROUNDS if
 *                 rounds <= 0.
 * \param output   the SHA256-Crypt result
 */
void sha2_crypt(const void *input, size_t ilen, const void *salt, size_t slen,
                uint32_t rounds, sb_t *output);

/**
 * \brief          Parse a SHA256-Crypt result
 *
 * \param input    the input to parse
 * \param rounds   the integer where the rounds number will be put. Could be
 *                 NULL if useless.
 * \param salt     the pstream where the salt will be put. Could be NULL if
 *                 useless.
 * \param hash     the pstream where the hash part will be put. Could be
 *                 NULL if useless.
 * \return         0 if successful, -1 if the hash has a bad format.
 */
int sha2_crypt_parse(lstr_t input, int *rounds, pstream_t *salt,
                     pstream_t *hash);

#ifdef __cplusplus
}
#endif

#endif /* sha2.h */
