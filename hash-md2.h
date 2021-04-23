/**
 * \file md2.h
 */
#ifndef XYSSL_MD2_H
#define XYSSL_MD2_H

/**
 * \brief          MD2 context structure
 */
typedef struct {
    byte cksum[16];    /*!< checksum of the data block */
    byte state[48];    /*!< intermediate digest state  */
    byte buffer[16];   /*!< data block being processed */

    byte ipad[64];     /*!< HMAC: inner padding        */
    byte opad[64];     /*!< HMAC: outer padding        */
    int left;          /*!< amount of data in buffer   */
} md2_ctx;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          MD2 context setup
 *
 * \param ctx      context to be initialized
 */
void md2_starts(md2_ctx * nonnull ctx) __leaf;

/**
 * \brief          MD2 process buffer
 *
 * \param ctx      MD2 context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void md2_update(md2_ctx * nonnull ctx, const void * nonnull input, ssize_t ilen)
    __leaf;

/**
 * \brief          MD2 final digest
 *
 * \param ctx      MD2 context
 * \param output   MD2 checksum result
 */
void md2_finish(md2_ctx * nonnull ctx, byte output[16]) __leaf;

/**
 * \brief          Output = MD2(input buffer)
 *
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   MD2 checksum result
 */
void md2(const void * nonnull input, ssize_t ilen, byte output[16]) __leaf;

/**
 * \brief          Output = MD2(file contents)
 *
 * \param path     input file name
 * \param output   MD2 checksum result
 *
 * \return         0 if successful, 1 if fopen failed,
 *                 or 2 if fread failed
 */
int md2_file(char * nonnull path, byte output[16]) __leaf;

/**
 * \brief          MD2 HMAC context setup
 *
 * \param ctx      HMAC context to be initialized
 * \param key      HMAC secret key
 * \param keylen   length of the HMAC key
 */
void md2_hmac_starts(md2_ctx * nonnull ctx, const void * nonnull key,
                     int keylen)
    __leaf;

/**
 * \brief          MD2 HMAC process buffer
 *
 * \param ctx      HMAC context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void md2_hmac_update(md2_ctx * nonnull ctx, const void * nonnull input,
                     ssize_t ilen)
    __leaf;

/**
 * \brief          MD2 HMAC final digest
 *
 * \param ctx      HMAC context
 * \param output   MD2 HMAC checksum result
 */
void md2_hmac_finish(md2_ctx * nonnull ctx, byte output[16]) __leaf;

/**
 * \brief          Output = HMAC-MD2(hmac key, input buffer)
 *
 * \param key      HMAC secret key
 * \param keylen   length of the HMAC key
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   HMAC-MD2 result
 */
void md2_hmac(const void * nonnull key, int keylen,
              const void * nonnull input, ssize_t ilen,
              byte output[16]) __leaf;

#ifdef __cplusplus
}
#endif

#endif /* md2.h */
