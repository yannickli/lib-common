/**
 * \file sha1.h
 */
#ifndef XYSSL_SHA1_H
#define XYSSL_SHA1_H

/**
 * \brief          SHA-1 context structure
 */
typedef struct {
    uint32_t total[2]; /*!< number of bytes processed  */
    uint32_t state[5]; /*!< intermediate digest state  */
    byte buffer[64];   /*!< data block being processed */

    byte ipad[64];     /*!< HMAC: inner padding        */
    byte opad[64];     /*!< HMAC: outer padding        */
} sha1_ctx;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          SHA-1 context setup
 *
 * \param ctx      context to be initialized
 */
void sha1_starts(sha1_ctx *ctx) __leaf;

/**
 * \brief          SHA-1 process buffer
 *
 * \param ctx      SHA-1 context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void sha1_update(sha1_ctx *ctx, const void *input, int ilen) __leaf;

/**
 * \brief          SHA-1 final digest
 *
 * \param ctx      SHA-1 context
 * \param output   SHA-1 checksum result
 */
void sha1_finish(sha1_ctx *ctx, byte output[20]) __leaf;

/**
 * \brief          SHA-1 final digest
 *
 * \param ctx      SHA-1 context
 * \param output   SHA-1 checksum result
 */
void sha1_finish_hex(sha1_ctx *ctx, char output[41]) __leaf;

/**
 * \brief          Output = SHA-1(input buffer)
 *
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   SHA-1 checksum result
 */
void sha1(const void *input, int ilen, byte output[20]) __leaf;

/**
 * \brief          Output = SHA-1(file contents)
 *
 * \param path     input file name
 * \param output   SHA-1 checksum result
 *
 * \return         0 if successful, 1 if fopen failed,
 *                 or 2 if fread failed
 */
int sha1_file(char *path, byte output[20]) __leaf;

/**
 * \brief          SHA-1 HMAC context setup
 *
 * \param ctx      HMAC context to be initialized
 * \param key      HMAC secret key
 * \param keylen   length of the HMAC key
 */
void sha1_hmac_starts(sha1_ctx *ctx, const void *key, int keylen)
    __leaf;

/**
 * \brief          SHA-1 HMAC process buffer
 *
 * \param ctx      HMAC context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void sha1_hmac_update(sha1_ctx *ctx, const void *input, int ilen)
    __leaf;

/**
 * \brief          SHA-1 HMAC final digest
 *
 * \param ctx      HMAC context
 * \param output   SHA-1 HMAC checksum result
 */
void sha1_hmac_finish(sha1_ctx *ctx, byte output[20])
    __leaf;

/**
 * \brief          Output = HMAC-SHA-1(hmac key, input buffer)
 *
 * \param key      HMAC secret key
 * \param keylen   length of the HMAC key
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   HMAC-SHA-1 result
 */
void sha1_hmac(const void *key, int keylen, const void *input, int ilen,
               byte output[20]) __leaf;

/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if the test failed
 */
int sha1_self_test(int verbose);

#ifdef __cplusplus
}
#endif

#endif /* sha1.h */
