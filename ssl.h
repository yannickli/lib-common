/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
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
 * This file offers an interface which wrap the openssl AES-CBC & PKEY
 * interfaces.
 *
 * \section AES-CBS
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
 * \section PKEY
 *
 * \subsection encryption
 *
 * The generic way to performs encryption is to init the SSL context with your
 * public key (or private if you have access to it, although it's not required
 * for encryption), call encrypt() on your data, and wipe the context.
 *
 * Example:
 *
 * ssl_ctx_t ctx;
 * if (!ssl_ctx_init_pkey(&ctx, LSTR_NULL_V, pub_key, LSTR_NULL_V)) {
 *     return logger_error(… "key loading error: %s", ssl_get_error());
 * }
 * if (ssl_encrypt(&ctx, msg, &msg_encrypted) < 0) {
 *     ssl_ctx_wipe(&ctx);
 *     return logger_error(… "decrypt error: %s", ssl_get_error());
 * }
 * ssl_ctx_wipe(&ctx);
 *
 * \subsection decryption
 *
 * Decryption works in the same way, except that you need the private key and
 * the passphrase if this key have been created with one.
 *
 * Example:
 *
 * ssl_ctx_t ctx;
 * if (!ssl_ctx_init_pkey(&ctx, priv_key, LSTR_NULL_V, pass)) {
 *     return logger_error(… "key loading error: %s", ssl_get_error());
 * }
 * if (ssl_decrypt(&ctx, msg_encrypted, &msg_clear) < 0) {
 *     ssl_ctx_wipe(&ctx);
 *     return logger_error(… "decrypt error: %s", ssl_get_error());
 * }
 * ssl_ctx_wipe(&ctx);
 *
 */

#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/engine.h>
#include <openssl/rsa.h>
#include "core.h"
#include "licence.h"

#if (OPENSSL_VERSION_NUMBER >= 0x0100000fL)
# define SSL_HAVE_EVP_PKEY
#endif

/* Encryption {{{ */

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
    /* CIPHER data */
    const EVP_CIPHER  *type;
    const EVP_MD      *md;

    EVP_CIPHER_CTX     *encrypt;
    EVP_CIPHER_CTX     *decrypt;

#ifdef SSL_HAVE_EVP_PKEY
    /* PKEY data */
    EVP_PKEY          *pkey;
    EVP_PKEY_CTX      *pkey_encrypt;
    EVP_PKEY_CTX      *pkey_decrypt;
#endif

    /* common data */
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
 * Init the given SSL context with the given key. You can use private or
 * public key to init the context, depending on what you need.
 *
 * You don't need to call ssl_ctx_init() it will be done for you.
 *
 * \param ctx       The SSL context.
 * \param priv_key  The private key.
 * \param pub_key   The public key.
 * \param pass      The passphrase.
 *
 * \return The initialized context or NULL in case of error.
 */
__attr_nonnull__((1))
ssl_ctx_t *ssl_ctx_init_pkey(ssl_ctx_t *ctx,
                             lstr_t priv_key, lstr_t pub_key,
                             lstr_t pass);

/**
 * Init the given SSL context with the public key.
 *
 * \warning You won't be able to decrypt data using this context.
 *
 * You don't need to call ssl_ctx_init() it will be done for you.
 *
 * \param ctx       The SSL context.
 * \param pub_key   The public key.
 *
 * \return The initialized context or NULL in case of error.
 */
__attr_nonnull__((1)) static inline
ssl_ctx_t *ssl_ctx_init_pkey_pub(ssl_ctx_t *ctx, lstr_t pub_key)
{
    return ssl_ctx_init_pkey(ctx, LSTR_EMPTY_V, pub_key, LSTR_EMPTY_V);

}

/**
 * Init the given SSL context with the private key and the passphrase.
 *
 * You don't need to call ssl_ctx_init() it will be done for you.
 *
 * \param ctx       The SSL context.
 * \param priv_key  The private key.
 * \param pass      The passphrase.
 *
 * \return The initialized context or NULL in case of error.
 */
__attr_nonnull__((1)) static inline
ssl_ctx_t *ssl_ctx_init_pkey_priv(ssl_ctx_t *ctx,
                                  lstr_t priv_key, lstr_t pass)
{
    return ssl_ctx_init_pkey(ctx, priv_key, LSTR_EMPTY_V, pass);
}

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
 * Encrypt the given data via public key and append the result to out.
 */
__must_check__ int
ssl_encrypt_pkey(ssl_ctx_t *ctx, lstr_t data, sb_t *out);

/**
 * Encrypt a bunch of data in one operation. The SSL context will be ready to
 * be updated again.
 */
__must_check__ static inline int
ssl_encrypt(ssl_ctx_t *ctx, lstr_t data, sb_t *out)
{
#ifdef SSL_HAVE_EVP_PKEY
    if (ctx->pkey) {
        RETHROW(ssl_encrypt_pkey(ctx, data, out));
        return 0;
    }
#endif
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
 * Decrypt the given data via private key and append the result to out.
 */
__must_check__ int
ssl_decrypt_pkey(ssl_ctx_t *ctx, lstr_t data, sb_t *out);

/**
 * Decrypt a bunch of data in one operation. The SSL context will be ready to
 * be updated again.
 */
__must_check__ static inline int
ssl_decrypt(ssl_ctx_t *ctx, lstr_t data, sb_t *out)
{
#ifdef SSL_HAVE_EVP_PKEY
    if (ctx->pkey) {
        RETHROW(ssl_decrypt_pkey(ctx, data, out));
        return 0;
    }
#endif
    RETHROW(ssl_decrypt_update(ctx, data, out));
    RETHROW(ssl_decrypt_reset(ctx, out));
    return 0;
}

/* }}} */
/* {{{ Signature */

#ifdef SSL_HAVE_EVP_PKEY

#ifdef __has_blocks
typedef int (BLOCK_CARET pem_password_b)(char *buf, int size, int rwflag);
#else
typedef void *pem_password_b;
#endif

typedef enum rsa_hash_algo_t {
    RSA_HASH_SHA256,
} rsa_hash_algo_t;

typedef struct rsa_sign_t rsa_sign_t;

rsa_sign_t * nullable rsa_sign_new(lstr_t priv_key, rsa_hash_algo_t algo,
                                   pem_password_b nullable pass_cb);

__must_check__
int rsa_sign_update(rsa_sign_t * nonnull ctx, const void * nonnull input,
                    int ilen);

__must_check__
int rsa_sign_finish(rsa_sign_t * nonnull * nonnull ctx, sb_t *out);

__must_check__
int rsa_sign_finish_hex(rsa_sign_t * nonnull * nonnull ctx, sb_t *out);


typedef struct rsa_verif_t rsa_verif_t;

__must_check__
rsa_verif_t * nullable rsa_verif_new(lstr_t pub_key, rsa_hash_algo_t algo,
                                     lstr_t bin_sig,
                                     pem_password_b nullable pass_cb);

__must_check__
rsa_verif_t * nullable rsa_verif_hex_new(lstr_t pub_key, rsa_hash_algo_t algo,
                                         lstr_t hex_sig,
                                         pem_password_b nullable pass_cb);

__must_check__
int rsa_verif_update(rsa_verif_t * nonnull ctx, const void * nonnull input,
                     int ilen);

__must_check__
int rsa_verif_finish(rsa_verif_t * nonnull * nonnull ctx);

/** Generate a RSA signature of an IOP structure with a private key.
 *
 * This function computes a signature from the IOP structure using the
 * provided private key. The public key associated with the private key is
 * required in order to check the signature.
 *
 * \param[in] st     IOP structure description.
 * \param[in] v      IOP structure to sign.
 * \param[in] priv_key The private key, in PEM format.
 * \param[in] flags  Flags modifying the hashing algorithm. The same flags
 *                   must be used when computing and checking the signature.
 * \param[in] pass_cb Block called to retrieve the password used to decrypt
 *                    the key in case the key is encrypted.
 */
lstr_t t_iop_compute_rsa_signature(const iop_struct_t * nonnull st,
                                   const void * nonnull v, lstr_t priv_key,
                                   unsigned flags,
                                   pem_password_b nullable pass_cb);

/** Check the RSA signature of an IOP structure with a public key.
 *
 * This function checks the signature of an IOP structure that way signed
 * using the private key associated to the \p pub_key public key.
 *
 * \param[in] st     IOP structure description.
 * \param[in] v      IOP structure to check.
 * \param[in] pub_key The public key, in PEM format.
 * \param[in] sig    Excepted signature.
 * \param[in] flags  Flags modifying the hashing algorithm. The same flags
 *                   must be used when computing and checking the signature.
 * \param[in] pass_cb Block called to retrieve the password used to decrypt
 *                    the key in case the key is encrypted.
 */
__must_check__
int iop_check_rsa_signature(const iop_struct_t * nonnull st,
                            const void * nonnull v, lstr_t pub_key,
                            lstr_t sig, unsigned flags,
                            pem_password_b nullable pass_cb);

#endif

/* }}} */
/* {{{ Licence */
/* ---- misc SSL usages for others modules ---- */

/**
 * Encrypt the given plaintext encryption key using the licence signature.
 */
char *licence_compute_encryption_key(const char *signature, const char *key);

/**
 * Decrypt the encryption key from a licence file.
 */
int licence_resolve_encryption_key(const conf_t *conf, sb_t *out);

#ifdef SSL_HAVE_EVP_PKEY

/** Check an IOP Licence signed using a RSA key.
 *
 * \warning this function won't check the licence expiry: use
 *          \ref licence_check_iop_expiry for that.
 *
 * \param[in] licence     The signed licence structure.
 * \param[in] version     The version of the product we're running on,
 *                        LSTR_NULL if the version should not be checked.
 * \param[in] pub_key     The public key in PEM format of the RSA key that was
 *                        used to sign he licence.
 * \param[in] flags       Flags to use to compute the signature.
 * \param[in] pass_cb     A callback used to retrieve the password of \p
 *                        pub_key in case it is encrypted.
 */
__must_check__
int licence_check_rsa(const struct core__signed_licence__t *licence,
                      lstr_t version, lstr_t pub_key, unsigned flags,
                      pem_password_b nullable pass_cb);
#endif

/* }}} */
/* Module {{{ */

MODULE_DECLARE(ssl);

/* }}} */
#endif
