/**
 * \file md4.h
 */
#ifndef XYSSL_MD4_H
#define XYSSL_MD4_H

/**
 * \brief          MD4 context structure
 */
typedef struct {
    uint32_t total[2]; /*!< number of bytes processed  */
    uint32_t state[4]; /*!< intermediate digest state  */
    byte buffer[64];   /*!< data block being processed */

    byte ipad[64];     /*!< HMAC: inner padding        */
    byte opad[64];     /*!< HMAC: outer padding        */
} md4_ctx;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          MD4 context setup
 *
 * \param ctx      context to be initialized
 */
void md4_starts(md4_ctx * nonnull ctx) __leaf;

/**
 * \brief          MD4 process buffer
 *
 * \param ctx      MD4 context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void md4_update(md4_ctx * nonnull ctx, const void * nonnull input, ssize_t ilen)
    __leaf;

/**
 * \brief          MD4 final digest
 *
 * \param ctx      MD4 context
 * \param output   MD4 checksum result
 */
void md4_finish(md4_ctx * nonnull ctx, byte output[16]) __leaf;

/**
 * \brief          Output = MD4(input buffer)
 *
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   MD4 checksum result
 */
void md4(const void * nonnull input, ssize_t ilen, byte output[16]) __leaf;

/**
 * \brief          Output = MD4(file contents)
 *
 * \param path     input file name
 * \param output   MD4 checksum result
 *
 * \return         0 if successful, 1 if fopen failed,
 *                 or 2 if fread failed
 */
int md4_file(char * nonnull path, byte output[16]) __leaf;

/**
 * \brief          MD4 HMAC context setup
 *
 * \param ctx      HMAC context to be initialized
 * \param key      HMAC secret key
 * \param keylen   length of the HMAC key
 */
void md4_hmac_starts(md4_ctx * nonnull ctx, const void * nonnull key,
                     int keylen)
    __leaf;

/**
 * \brief          MD4 HMAC process buffer
 *
 * \param ctx      HMAC context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void md4_hmac_update(md4_ctx * nonnull ctx, const void * nonnull input,
                     ssize_t ilen)
    __leaf;

/**
 * \brief          MD4 HMAC final digest
 *
 * \param ctx      HMAC context
 * \param output   MD4 HMAC checksum result
 */
void md4_hmac_finish(md4_ctx * nonnull ctx, byte output[16])
    __leaf;

/**
 * \brief          Output = HMAC-MD4(hmac key, input buffer)
 *
 * \param key      HMAC secret key
 * \param keylen   length of the HMAC key
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   HMAC-MD4 result
 */
void md4_hmac(const void * nonnull key, int keylen,
              const void * nonnull input, ssize_t ilen,
              byte output[16]) __leaf;

#ifdef __cplusplus
}
#endif

#endif /* md4.h */
