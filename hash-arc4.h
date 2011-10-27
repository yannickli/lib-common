/**
 * \file arc4.h
 */
#ifndef XYSSL_ARC4_H
#define XYSSL_ARC4_H

/**
 * \brief          ARC4 context structure
 */
typedef struct {
    int x;             /*!< permutation index */
    int y;             /*!< permutation index */
    byte m[256];       /*!< permutation table */
} arc4_ctx;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          ARC4 key schedule
 *
 * \param ctx      ARC4 context to be initialized
 * \param key      the secret key
 * \param keylen   length of the key
 */
void arc4_setup(arc4_ctx *ctx, const byte *key, int keylen)
    __leaf;

/**
 * \brief          ARC4 cipher function
 *
 * \param ctx      ARC4 context
 * \param buf      buffer to be processed
 * \param buflen   amount of data in buf
 */
void arc4_crypt(arc4_ctx *ctx, byte *buf, int buflen)
    __leaf;

#ifdef __cplusplus
}
#endif

#endif /* arc4.h */
