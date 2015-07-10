/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2015 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_SSL_H
#define IS_LIB_COMMON_SSL_H

/**
 * This file offers an interface which wrap the openssl AES-CBC interface.
 *
 * Basics
 * ======
 *
 * The generic way to performs encrypt is to get an SSL context using one of
 * the initializer, then update() the encrypted data as much as you want and
 * finally call the finish() function to close the flow. Using the reset()
 * operation you could repeat these operations several times. Example:
 *
 * // Get the SSL context
 * init_aes256();
 *
 * // Encrypt some data
 * update();
 * update();
 * update();
 *
 * // Prepare to encrypt some others data
 * reset();
 *
 * // Encrypt the others data
 * update();
 * update();
 *
 * // Close the flow and wipe the SSL context
 * finish();
 * wipe();
 *
 * Decryption works in the same way.
 *
 */

#include <openssl/evp.h>
#include <openssl/aes.h>
#include "core.h"
#include "licence.h"

enum ssl_ctx_state {
    SSL_CTX_NONE,
    SSL_CTX_INIT,
    SSL_CTX_UPDATE,
    SSL_CTX_FINISH,
};

/**
 * SSL context used to encrypt and decrypt data.
 */
typedef struct ssl_ctx_t {
    const EVP_CIPHER  *type;
    const EVP_MD      *md;

    EVP_CIPHER_CTX     encrypt;
    EVP_CIPHER_CTX     decrypt;

    enum ssl_ctx_state encrypt_state;
    enum ssl_ctx_state decrypt_state;
} ssl_ctx_t;

void ssl_ctx_wipe(ssl_ctx_t *ctx);
GENERIC_DELETE(ssl_ctx_t, ssl_ctx);

/**
 * Init the SSL context with the given key and an optional salt. This
 * initializer will use AES 256 with SHA256.
 *
 * You don't need to call ssl_ctx_init() it will be done for you.
 *
 * \param ctx       The SSL context.
 * \param key       The cypher key to use for encryption.
 * \param salt      The salt to use when encrypting.
 * \param nb_rounds The iteration count to use, changing this value will break
 *                  encryption/decryption compatibility (a value of 1024
 *                  should be good in most situations).
 *
 * \return The initialized AES context or NULL in case of error.
 */
ssl_ctx_t *ssl_ctx_init_aes256(ssl_ctx_t *ctx, lstr_t key, uint64_t salt,
                               int nb_rounds);

/**
 * Same as ssl_ctx_init_aes() but allocate the ssl_ctx_t for you.
 */
static inline ssl_ctx_t *
ssl_ctx_new_aes256(lstr_t key, uint64_t salt, int nb_rounds)
{
    ssl_ctx_t *ctx = p_new_raw(ssl_ctx_t, 1);

    if (unlikely(!ssl_ctx_init_aes256(ctx, key, salt, nb_rounds))) {
        p_delete(&ctx);
    }

    return ctx;
}

/**
 * Reset the whole SSL context changing the key, the salt and the nb_rounds
 * parameters.
 */
int ssl_ctx_reset(ssl_ctx_t *ctx, lstr_t key, uint64_t salt, int nb_rounds);

/**
 * Retrieve the last SSL error in the current thread.
 */
const char *ssl_get_error(void);

/**
 * Encrypt the given data and put the result in out.
 */
__must_check__ int ssl_encrypt_update(ssl_ctx_t *ctx, lstr_t data, sb_t *out);

/**
 * Finalize the encrypted buffer.
 */
__must_check__ int ssl_encrypt_finish(ssl_ctx_t *ctx, sb_t *out);

/**
 * Reset the SSL context for the next data to encrypt using the same salt as
 * preceding. This function will call ssl_encrypt_finish() if needed. The out
 * parameter is mandatory in this case.
 */
__must_check__ int ssl_encrypt_reset(ssl_ctx_t *ctx, sb_t *out);

/**
 * Just like ssl_encrypt_reset() this function will allow to encrypt a new
 * bunch of data but you can change the key, the salt and the nb_rounds
 * parameters.
 */
__must_check__ int
ssl_encrypt_reset_full(ssl_ctx_t *ctx, sb_t *out, lstr_t key, uint64_t salt,
                       int nb_rounds);

/**
 * Encrypt a bunch of data in one operation. The SSL context will be ready to
 * be updated again.
 */
__must_check__ static inline int
ssl_encrypt(ssl_ctx_t *ctx, lstr_t data, sb_t *out)
{
    RETHROW(ssl_encrypt_update(ctx, data, out));
    RETHROW(ssl_encrypt_reset(ctx, out));

    return 0;
}


/**
 * Decrypt the given data and put the result in out.
 */
__must_check__ int ssl_decrypt_update(ssl_ctx_t *ctx, lstr_t data, sb_t *out);

/**
 * Finalize the decrypted buffer.
 */
__must_check__ int ssl_decrypt_finish(ssl_ctx_t *ctx, sb_t *out);

/**
 * Reset the SSL context for the next data to decrypt using the same salt as
 * preceding. This function will call ssl_decrypt_finish() if needed. The out
 * parameter is mandatory in this case.
 */
__must_check__ int ssl_decrypt_reset(ssl_ctx_t *ctx, sb_t *out);

/**
 * Just like ssl_decrypt_reset() this function will allow to decrypt a new
 * bunch of data but you can change the key, the salt and the nb_rounds
 * parameters.
 */
__must_check__ int
ssl_decrypt_reset_full(ssl_ctx_t *ctx, sb_t *out, lstr_t key, uint64_t salt,
                       int nb_rounds);

/**
 * Decrypt a bunch of data in one operation. The SSL context will be ready to
 * be updated again.
 */
__must_check__ static inline int
ssl_decrypt(ssl_ctx_t *ctx, lstr_t data, sb_t *out)
{
    RETHROW(ssl_decrypt_update(ctx, data, out));
    RETHROW(ssl_decrypt_reset(ctx, out));

    return 0;
}


/* ---- misc SSL usages for others modules ---- */

/**
 * Encrypt the given plaintext encryption key using the licence signature.
 */
char *licence_compute_encryption_key(const char *signature, const char *key);

/**
 * Decrypt the encryption key from a licence file.
 */
int licence_resolve_encryption_key(const conf_t *conf, sb_t *out);

/* Module {{{ */

MODULE_DECLARE(ssl);

/* }}} */
#endif
