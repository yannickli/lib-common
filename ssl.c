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

#include <openssl/err.h>
#include <lib-common/z.h>
#include "ssl.h"

/* {{{ Module */

static int ssl_initialize(void *arg)
{
    /* interpreting internal errors is a nightmare, we want human readable
     * messages */
    ERR_load_crypto_strings();
    return 0;
}

static int ssl_shutdown(void)
{
    ERR_free_strings();
    return 0;
}

MODULE_BEGIN(ssl)
MODULE_END()

/* }}} */
/* {{{ Initialize & wipe */

static __thread char openssl_errbuf_g[1024];

#define __ssl_set_error(msg, ...)                                            \
    ({ snprintf(openssl_errbuf_g, sizeof(openssl_errbuf_g),                  \
                msg, ##__VA_ARGS__);                                         \
       -1; })

#define ssl_set_error(msg) \
    __ssl_set_error(msg ": %s", ERR_error_string(ERR_get_error(), NULL))


static ssl_ctx_t *ssl_ctx_init(ssl_ctx_t *ctx)
{
    p_clear(ctx, 1);

    EVP_CIPHER_CTX_init(&ctx->encrypt);
    EVP_CIPHER_CTX_init(&ctx->decrypt);

    return ctx;
}
GENERIC_NEW(ssl_ctx_t, ssl_ctx);

void ssl_ctx_wipe(ssl_ctx_t *ctx)
{
#ifndef NDEBUG
    if (unlikely(ctx->encrypt_state == SSL_CTX_UPDATE))
        e_trace(0, "SSL context closed with inconsistent encrypt state");
    if (unlikely(ctx->decrypt_state == SSL_CTX_UPDATE))
        e_trace(0, "SSL context closed with inconsistent decrypt state");
#endif
#ifdef SSL_HAVE_EVP_PKEY
    if (ctx->pkey_encrypt) {
        EVP_PKEY_CTX_free(ctx->pkey_encrypt);
        ctx->pkey_encrypt = NULL;
    }
    if (ctx->pkey_decrypt) {
        EVP_PKEY_CTX_free(ctx->pkey_decrypt);
        ctx->pkey_decrypt = NULL;
    }
    if (ctx->pkey) {
        EVP_PKEY_free(ctx->pkey);
        ctx->pkey = NULL;
    }
#endif

    EVP_CIPHER_CTX_cleanup(&ctx->encrypt);
    EVP_CIPHER_CTX_cleanup(&ctx->decrypt);
}

static int ssl_ctx_configure(ssl_ctx_t *ctx, const lstr_t key,
                             const uint64_t salt, const int nb_rounds,
                             const bool do_encrypt, const bool do_decrypt)
{
    t_scope;
    unsigned char *keybuf, *ivbuf;

    assert (do_encrypt || do_decrypt);

    keybuf = t_new_raw(unsigned char, EVP_CIPHER_key_length(ctx->type));
    ivbuf  = t_new_raw(unsigned char, EVP_CIPHER_iv_length(ctx->type));

    /* Generate the key and IV from the given key and salt. */
    EVP_BytesToKey(ctx->type, ctx->md, (const unsigned char *)&salt,
                   (const unsigned char *)key.s, key.len, nb_rounds, keybuf,
                   ivbuf);

    if (do_encrypt) {
        if (unlikely(!EVP_EncryptInit_ex(&ctx->encrypt, ctx->type, NULL,
                                         keybuf, ivbuf)))
        {
            return ssl_set_error("encrypt context initialization failure");
        }
        ctx->encrypt_state = SSL_CTX_INIT;
    }

    if (do_decrypt) {
        if (unlikely(!EVP_DecryptInit_ex(&ctx->decrypt, ctx->type, NULL,
                                         keybuf, ivbuf)))
        {
            return ssl_set_error("decrypt context initialization failure");
        }
        ctx->decrypt_state = SSL_CTX_INIT;
    }

    return 0;
}

ssl_ctx_t *ssl_ctx_init_aes256(ssl_ctx_t *ctx, lstr_t key, uint64_t salt,
                               int nb_rounds)
{
    ssl_ctx_init(ctx);

    ctx->type = EVP_aes_256_cbc();
    ctx->md   = EVP_sha256();

    if (ssl_ctx_configure(ctx, key, salt, nb_rounds, true, true) < 0)
    {
        ssl_ctx_wipe(ctx);
        return NULL;
    }

    return ctx;
}

ssl_ctx_t *ssl_ctx_init_pkey(ssl_ctx_t *ctx,
                             lstr_t priv_key, lstr_t pub_key, lstr_t pass)
{
#ifdef SSL_HAVE_EVP_PKEY
    t_scope;
    lstr_t key = priv_key.len ? priv_key : pub_key;
    void *u = pass.len ? t_dupz(pass.data, pass.len) : NULL;
    BIO *bio = NULL;

    ssl_ctx_init(ctx);

    if (!key.len) {
        __ssl_set_error("invalid key");
        goto error;
    }

    bio = BIO_new_mem_buf(key.v, key.len);
    if (!bio) {
        ssl_set_error("allocation failure");
        goto error;
    }

    if (priv_key.len) {
        ctx->pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, u);
    } else {
        ctx->pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, u);
    }
    if (!ctx->pkey) {
        ssl_set_error("read key failure");
        goto error;
    }
    BIO_free(bio);
    bio = NULL;

    /* whatever key we have, we can alyas encrypt data */
    ctx->pkey_encrypt = EVP_PKEY_CTX_new(ctx->pkey, NULL);
    if (!ctx->pkey_encrypt) {
        ssl_set_error("allocation failure");
        goto error;
    }

    if (priv_key.len) {
        /* private key allows us to decrypt data */
        ctx->pkey_decrypt = EVP_PKEY_CTX_dup(ctx->pkey_encrypt);
        if (!ctx->pkey_decrypt) {
            ssl_set_error("allocation failure");
            goto error;
        }
        if (EVP_PKEY_decrypt_init(ctx->pkey_decrypt) <= 0) {
            ssl_set_error("decrypt context initialization failure");
            goto error;
        }
        ctx->decrypt_state = SSL_CTX_INIT;
    }

    if (EVP_PKEY_encrypt_init(ctx->pkey_encrypt) <= 0) {
        ssl_set_error("encrypt context initialization failure");
        goto error;
    }
    ctx->encrypt_state = SSL_CTX_INIT;

    return ctx;

  error:
    if (bio) {
        BIO_free(bio);
    }
    ssl_ctx_wipe(ctx);
    return NULL;
#else
    __ssl_set_error("private key not supported");
    ssl_ctx_wipe(ctx);
    return NULL;
#endif
}

int ssl_ctx_reset(ssl_ctx_t *ctx, lstr_t key, uint64_t salt, int nb_rounds)
{
    assert (ctx->encrypt_state == SSL_CTX_NONE
        ||  ctx->encrypt_state == SSL_CTX_INIT
        ||  ctx->encrypt_state == SSL_CTX_FINISH);
    assert (ctx->decrypt_state == SSL_CTX_NONE
        ||  ctx->decrypt_state == SSL_CTX_INIT
        ||  ctx->decrypt_state == SSL_CTX_FINISH);

    return ssl_ctx_configure(ctx, key, salt, nb_rounds, true, true);
}

const char *ssl_get_error(void)
{
    return openssl_errbuf_g;
}

/* }}} */
/* {{{ CYPHER */
/* {{{ Encrypt */

int ssl_encrypt_update(ssl_ctx_t *ctx, lstr_t data, sb_t *out)
{
    int clen = data.len + EVP_CIPHER_CTX_block_size(&ctx->encrypt) + 1;

    assert (ctx->encrypt_state == SSL_CTX_INIT
        ||  ctx->encrypt_state == SSL_CTX_UPDATE);

    if (unlikely(data.len <= 0)) {
        /* XXX with openssl < 1.0, EVP_EncryptUpdate will abort() with empty
         * data. */
        ctx->encrypt_state = SSL_CTX_UPDATE;
        return 0;
    }

    if (unlikely(!EVP_EncryptUpdate(&ctx->encrypt,
                                    (unsigned char *)sb_grow(out, clen),
                                    &clen, (unsigned char *)data.s,
                                    data.len)))
    {
        ctx->encrypt_state = SSL_CTX_NONE;
        return ssl_set_error("encrypt error");
    }

    assert (sb_avail(out) > clen);
    sb_growlen(out, clen);
    ctx->encrypt_state = SSL_CTX_UPDATE;

    return 0;
}

int ssl_encrypt_finish(ssl_ctx_t *ctx, sb_t *out)
{
    int outlen = 0;

    assert (ctx->encrypt_state == SSL_CTX_UPDATE);

    if (unlikely(!EVP_EncryptFinal_ex(&ctx->encrypt,
                                      (unsigned char *)sb_end(out), &outlen)))
    {
        ctx->encrypt_state = SSL_CTX_NONE;
        return ssl_set_error("encrypt finalization error");
    }

    assert (sb_avail(out) > outlen);
    sb_growlen(out, outlen);
    ctx->encrypt_state = SSL_CTX_FINISH;

    return 0;
}

int ssl_encrypt_reset(ssl_ctx_t *ctx, sb_t *out)
{
    assert (ctx->encrypt_state != SSL_CTX_NONE);

    if (ctx->encrypt_state == SSL_CTX_UPDATE) {
        assert (out);
        RETHROW(ssl_encrypt_finish(ctx, out));
    }

    if (ctx->encrypt_state == SSL_CTX_FINISH) {
        if (unlikely(!EVP_EncryptInit_ex(&ctx->encrypt, NULL, NULL, NULL,
                                         NULL)))
        {
            ctx->encrypt_state = SSL_CTX_NONE;
            return ssl_set_error("encrypt reset error");
        }
        ctx->encrypt_state = SSL_CTX_INIT;
    }
    assert (ctx->encrypt_state == SSL_CTX_INIT);

    return 0;
}

int ssl_encrypt_reset_full(ssl_ctx_t *ctx, sb_t *out, lstr_t key,
                           uint64_t salt, int nb_rounds)
{
    assert (ctx->encrypt_state != SSL_CTX_NONE);

    if (ctx->encrypt_state == SSL_CTX_UPDATE) {
        assert (out);
        RETHROW(ssl_encrypt_finish(ctx, out));
    }
    assert (ctx->encrypt_state == SSL_CTX_INIT);

    return ssl_ctx_configure(ctx, key, salt, nb_rounds, true, false);
}

/* }}} */
/* {{{ Decrypt */

int ssl_decrypt_update(ssl_ctx_t *ctx, lstr_t data, sb_t *out)
{
    int clen = data.len + EVP_CIPHER_CTX_block_size(&ctx->decrypt) + 1;

    assert (ctx->decrypt_state == SSL_CTX_INIT
        ||  ctx->decrypt_state == SSL_CTX_UPDATE);

    if (unlikely(!EVP_DecryptUpdate(&ctx->decrypt,
                                    (unsigned char *)sb_grow(out, clen),
                                    &clen, (unsigned char *)data.s,
                                    data.len)))
    {
        ctx->decrypt_state = SSL_CTX_NONE;
        return ssl_set_error("decrypt error");
    }

    assert (sb_avail(out) > clen);
    sb_growlen(out, clen);
    ctx->decrypt_state = SSL_CTX_UPDATE;

    return 0;
}

int ssl_decrypt_finish(ssl_ctx_t *ctx, sb_t *out)
{
    int outlen = 0;

    assert (ctx->decrypt_state == SSL_CTX_UPDATE);

    if (unlikely(!EVP_DecryptFinal_ex(&ctx->decrypt,
                                      (unsigned char *)sb_end(out), &outlen)))
    {
        ctx->decrypt_state = SSL_CTX_NONE;
        return ssl_set_error("decrypt finalization error");
    }

    assert (sb_avail(out) > outlen);
    sb_growlen(out, outlen);
    ctx->decrypt_state = SSL_CTX_FINISH;

    return 0;
}

int ssl_decrypt_reset(ssl_ctx_t *ctx, sb_t *out)
{
    assert (ctx->decrypt_state != SSL_CTX_NONE);

    if (ctx->decrypt_state == SSL_CTX_UPDATE) {
        assert (out);
        RETHROW(ssl_decrypt_finish(ctx, out));
    }

    if (ctx->decrypt_state == SSL_CTX_FINISH) {
        if (unlikely(!EVP_DecryptInit_ex(&ctx->decrypt, NULL, NULL, NULL,
                                         NULL)))
        {
            ctx->decrypt_state = SSL_CTX_NONE;
            return ssl_set_error("decrypt reset error");
        }
        ctx->decrypt_state = SSL_CTX_INIT;
    }
    assert (ctx->decrypt_state == SSL_CTX_INIT);

    return 0;
}

int ssl_decrypt_reset_full(ssl_ctx_t *ctx, sb_t *out, lstr_t key,
                           uint64_t salt, int nb_rounds)
{
    assert (ctx->decrypt_state != SSL_CTX_NONE);

    if (ctx->decrypt_state == SSL_CTX_UPDATE) {
        assert (out);
        RETHROW(ssl_decrypt_finish(ctx, out));
    }
    assert (ctx->decrypt_state == SSL_CTX_INIT);

    return ssl_ctx_configure(ctx, key, salt, nb_rounds, false, true);
}

/* }}} */
/* }}} */
/* {{{ PKEY */
/* {{{ Encrypt */

int ssl_encrypt_pkey(ssl_ctx_t *ctx, lstr_t data, sb_t *out)
{
#ifdef SSL_HAVE_EVP_PKEY
    size_t outlen;
    size_t inlen = data.len;
    const unsigned char *in = data.data;

    if (!ctx->pkey
    ||  !ctx->pkey_encrypt
    ||  ctx->encrypt_state != SSL_CTX_INIT)
    {
        __ssl_set_error("invalid encrypt context state");
        return -1;
    }

    if (EVP_PKEY_encrypt(ctx->pkey_encrypt, NULL, &outlen, in, inlen) <= 0)
    {
        ssl_set_error("decryption failure (size computation)");
        return -1;
    }
    if (EVP_PKEY_encrypt(ctx->pkey_encrypt,
                         (unsigned char *)sb_grow(out, outlen),
                         &outlen, in, inlen) <= 0)
    {
        ssl_set_error("decryption failure");
        return -1;
    }
    sb_growlen(out, outlen);
    return 0;
#else
    __ssl_set_error("private key not supported");
    return -1;
#endif
}

/* }}} */
/* {{{ Decrypt */

int ssl_decrypt_pkey(ssl_ctx_t *ctx, lstr_t data, sb_t *out)
{
#ifdef SSL_HAVE_EVP_PKEY
    size_t outlen;
    size_t inlen = data.len;
    const unsigned char *in = data.data;

    if (!ctx->pkey
    ||  !ctx->pkey_decrypt
    ||  ctx->decrypt_state != SSL_CTX_INIT)
    {
        __ssl_set_error("invalid decrypt context state");
        return -1;
    }

    if (EVP_PKEY_decrypt(ctx->pkey_decrypt, NULL, &outlen, in, inlen) <= 0)
    {
        ssl_set_error("decryption failure (size computation)");
        return -1;
    }
    if (EVP_PKEY_decrypt(ctx->pkey_decrypt,
                         (unsigned char *)sb_grow(out, outlen),
                         &outlen, in, inlen) <= 0)
    {
        ssl_set_error("decryption failure");
        return -1;
    }
    sb_growlen(out, outlen);
    return 0;
#else
    __ssl_set_error("private key not supported");
    return -1;
#endif
}

/* }}} */
/* }}} */
/* {{{ Licensing module */

char *licence_compute_encryption_key(const char *signature, const char *key)
{
    /* We encrypt the obfuscation key using the compute signature as a SSL
     * key. The salt will be 42 and that's all. */
    ssl_ctx_t ctx;
    char *encrypted;
    int len;
    SB_1k(sb);

    RETHROW_P(ssl_ctx_init_aes256(&ctx, LSTR(signature), 42424242424242ULL,
                                  1024));

    if (ssl_encrypt(&ctx, LSTR(key), &sb) < 0) {
        ssl_ctx_wipe(&ctx);
        return NULL;
    }

    /* Encode the encrypted key as hex-string */
    encrypted = sb_detach(&sb, &len);
    sb_add_hex(&sb, encrypted, len);
    p_delete(&encrypted);

    ssl_ctx_wipe(&ctx);
    return sb_detach(&sb, NULL);
}

int licence_resolve_encryption_key(const conf_t *conf, sb_t *out)
{
    ssl_ctx_t ctx;
    const char *signature, *key;
    int res = -1;
    SB_1k(sb);

    RETHROW_PN(signature = conf_get_raw(conf, "licence", "signature"));
    RETHROW_PN(key = conf_get_raw(conf, "licence", "encryptionKey"));

    RETHROW_PN(ssl_ctx_init_aes256(&ctx, LSTR(signature), 42424242424242ULL,
                                   1024));
    /* Decode the hex-string */
    if (sb_add_unhex(&sb, key, strlen(key)) < 0) {
        goto end;
    }

    /* Decrypt the key */
    if (ssl_decrypt(&ctx, LSTR_SB_V(&sb), out) < 0) {
        goto end;
    }

    res = 0;
  end:
    ssl_ctx_wipe(&ctx);

    return res;
}

/* }}} */
/* {{{ Tests */

Z_GROUP_EXPORT(ssl)
{
    MODULE_REQUIRE(ssl);

/* {{{ AES */

    Z_TEST(encrypt_aes, "encrypt_aes") {
        ssl_ctx_t ctx;
        const lstr_t text = LSTR_IMMED("Encrypt me");
        const lstr_t res  = LSTR_IMMED("\x12\x29\xB4\x7E\x75\xE1\x33\x64"
                                       "\x49\x09\x77\xF8\xE8\x08\x2C\x58");
        SB_1k(sb);

        Z_ASSERT_P(ssl_ctx_init_aes256(&ctx, LSTR("plop"), 42, 1024));

        Z_ASSERT_N(ssl_encrypt(&ctx, text, &sb));

        Z_ASSERT_LSTREQUAL(res, LSTR_SB_V(&sb));

        /* Non regression of #11267: check that encrypt with empty content to
         * not abort with openssl < v1.0 */
        {
            SB_1k(sb_dec);

            /* check that encrypt to not abort */
            sb_reset(&sb);
            Z_ASSERT_N(ssl_encrypt(&ctx, LSTR_EMPTY_V, &sb));

            /* check that decrypt will correctly decrypt the empty string */
            Z_ASSERT_N(ssl_decrypt(&ctx, LSTR_SB_V(&sb), &sb_dec));
            Z_ASSERT_LSTREQUAL(LSTR_SB_V(&sb_dec), LSTR_EMPTY_V);
        }

        ssl_ctx_wipe(&ctx);
    } Z_TEST_END;

    Z_TEST(decrypt_aes, "decrypt_aes") {
        ssl_ctx_t ctx;
        const lstr_t text = LSTR_IMMED("\x12\x29\xB4\x7E\x75\xE1\x33\x64"
                                       "\x49\x09\x77\xF8\xE8\x08\x2C\x58");
        SB_1k(sb);

        Z_ASSERT_P(ssl_ctx_init_aes256(&ctx, LSTR("plop"), 42, 1024));

        Z_ASSERT_N(ssl_decrypt(&ctx, text, &sb));

        Z_ASSERT_LSTREQUAL(LSTR("Encrypt me"), LSTR_SB_V(&sb));

        ssl_ctx_wipe(&ctx);
    } Z_TEST_END;

/* }}} */
/* {{{ PKEY */

#ifdef SSL_HAVE_EVP_PKEY
    {
        /* To generate such test data, you can:
         * $ openssl genpkey -algorithm RSA -out priv.pem -pkeyopt
         *   rsa_keygen_bits:2048
         * $ openssl pkey -in priv.pem -pubout -out pub.pem
         * $ echo 'secret pioupiou23' > a
         * $ openssl pkeyutl -encrypt -pubin -inkey pub.pem -in a -hexdump
         */
        const lstr_t priv  = LSTR_IMMED(
          "-----BEGIN PRIVATE KEY-----\n"
          "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCpwkBnUq70MGZL\n"
          "qlcm5AhAeEQ+jiicYOSJCN2c/rGCx+wwtcYO1tTQ3kR1/pM/stoJsWQgy7XVyu4d\n"
          "x0PhaTn6lCnnxaqSH8vE7kyZS/r1Lsrdy9J+MA28mikdTA5qq7L0fgDN52NwXY9Y\n"
          "hXxM+2VEdoU1S9gQ3ORf72kXHIEl4o3dqL4JywXfki+euo/PZ3axFk8YABmZboL3\n"
          "LVNsCiRTGw+O2kXjzCenlp2KRJQAEUOu9FClwHdC3gGsPF4t/OU6VmhACxYWTxup\n"
          "kLBFRJhtLrade/6w9YafCb8Ml7jTqPQ+Coe5jPDzyg7aC3MVMbjl+AdKdMAMBHy9\n"
          "BY7pQreVAgMBAAECggEAeHXwT6FbpsnFfUHl0CIWPPFas+0aokUbRqZ049fTzNLj\n"
          "JnmGjrchkwl2GSjKAnR+xkwLmj4TzR7QM29YGtcZnlePGPmqHUDUzuyujEVfUqae\n"
          "rB7bQlIFHWVjcXer70PhnB7hoTrl1DF/67flSZdG9/sGcZhdPTISGIWB1DWU63Ud\n"
          "exdTqfuVNzrw4mKYiL+Oekgs6jaAwP6edbjtpLfwKQiL7kjMDZ8OLef2d4c6yHIQ\n"
          "Cby2ZoFho6yd3UjSKWxEREMzXPy6H5Ai9qSF6oNSBGVT+dxAl1NvXtHGk8reDYMa\n"
          "6XSoQj/r2XYsgqhbxFtZRknckgnkTZgtrkFn93XDgQKBgQDU8Nwzymh4ZgZCCb7p\n"
          "7Z0JDa0n0/TALxccvb0Bxuvkf32HgIw4dOVF5GJyiOCp/1FivpXTk4fLsOHANmde\n"
          "FPme+nvOufl75l1aREnNRfWsfUNeiQQmCLXqszgHteZA4hY8Hibj9vJw5QtEoBv6\n"
          "ZkU9BHDJ5G6Y/tiO+vvwB8gWtQKBgQDMFgUWJmW7uh3qEAdfpQGj2EZtmSmaj9Dg\n"
          "W228M/sKmHOYobgRN9vj0k7hcqLqJHAZxXz6ktSybzXSX9CLZ+vohw5DoEKRFhZC\n"
          "ofV+0i8sMMqScM3ckWR4hLxdzkKdMUM6UhWPA13GwkoIm4IqhFUl/eeaE9+jog/2\n"
          "ghfxXubJYQKBgQCuzIGWqiMUInwknadwlDOCiQ2JUj7pvD42w7Jx0P83dUhwgR+a\n"
          "AKtsskv2RVJXelUuv9Bx+/tPRAYtKPu1iXZYALq9OoEIKlSbks8aiMFhNPqmkccs\n"
          "CZ576V6nRbSbsnwaIY4/OCpQblTPorcU1/siWZDUyoXXZewTgwhpQ5oGuQKBgB8v\n"
          "JivSRi0/LR78wAOvVObSP0Cz7JV/cC04CzZ8wtlFnjQuUc/ftyvCkOcF+zrHwpFN\n"
          "ieFH2lRBhfnVRipnALcRG+7daA5/T3ty/+4W87pO4kUqE2qmlLGNprK2t5sJUfpx\n"
          "XHXzz7p1KZbTHDqe6dvaRi9W5g88zi+ehUYOeDlBAoGAMU9h9A20PvuvH8y4lcyQ\n"
          "yXoTAIGUwMBZJ82WOMFq4xeyxo/i4XvKu9pWBNn1MArbjCJBwJ3BIqAwH7zF2Oz7\n"
          "c2kW/t1JmfPUYzW+Xq4XL4fy2sDBgw0tVdAiMViAQODuLrUeyVuku7F5hshID/Z2\n"
          "DmdLLTHk5H9Q6pZChKFO3c4=\n"
          "-----END PRIVATE KEY-----\n");
        const lstr_t pub  = LSTR_IMMED(
          "-----BEGIN PUBLIC KEY-----\n"
          "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAqcJAZ1Ku9DBmS6pXJuQI\n"
          "QHhEPo4onGDkiQjdnP6xgsfsMLXGDtbU0N5Edf6TP7LaCbFkIMu11cruHcdD4Wk5\n"
          "+pQp58Wqkh/LxO5MmUv69S7K3cvSfjANvJopHUwOaquy9H4AzedjcF2PWIV8TPtl\n"
          "RHaFNUvYENzkX+9pFxyBJeKN3ai+CcsF35IvnrqPz2d2sRZPGAAZmW6C9y1TbAok\n"
          "UxsPjtpF48wnp5adikSUABFDrvRQpcB3Qt4BrDxeLfzlOlZoQAsWFk8bqZCwRUSY\n"
          "bS62nXv+sPWGnwm/DJe406j0PgqHuYzw88oO2gtzFTG45fgHSnTADAR8vQWO6UK3\n"
          "lQIDAQAB\n"
          "-----END PUBLIC KEY-----\n");
        const lstr_t text = LSTR_IMMED("secret pioupiou23\n");
        const lstr_t text_garbage = LSTR_IMMED("bonjour openssl!\n");

/* {{{ PKEY for RSA without padding */

    Z_TEST(pkey_rsa_without_padding, "pkey_rsa_without_padding") {
        ssl_ctx_t ctx;
        /* We encrypt with -pkeyopt rsa_padding_mode:none */
        /* As the encrypted data is deterministic, we can check it */
        const lstr_t text_encrypted = LSTR_IMMED(
          "\x52\x6a\xcd\x65\x2a\xec\xe1\x2a\x0a\x55\x93\x96\x14\x5d\xd9\x3a"
          "\xb6\xce\x4d\xe0\xfa\x53\x6a\xd7\x9b\x34\x7b\xe9\x03\xcc\x07\xc6"
          "\x32\x8a\x55\x3a\x3a\x18\x28\x3b\x42\xeb\xa1\x7f\xcc\x84\x66\x7c"
          "\x46\x6e\x25\xf6\x75\xc4\xc1\x54\xca\x50\x0f\x50\xe8\x1a\x81\x81"
          "\x6f\xc3\xa3\x5f\xba\x18\x46\xf4\xc2\x74\x71\x59\xeb\x42\x10\x20"
          "\x76\xab\x00\x42\x71\x4e\x31\xad\xe9\x83\x03\x7b\x94\x16\x07\x88"
          "\x8e\x10\xd3\xe5\xcb\x31\x4d\x27\x71\xd3\x65\x1c\xcd\x3c\xc0\xc2"
          "\x81\x92\xdd\xfa\xc8\xb9\x40\xa8\x28\x6a\xf0\x57\x3d\xfc\xf9\x9c"
          "\xaf\xc2\x01\x2e\xae\x5c\xa5\xf4\xaa\x85\xa8\xeb\x13\x11\x60\xd9"
          "\x89\x61\x75\xd7\xfb\xdd\xab\x80\x59\x5b\x7f\x2e\x38\x1f\x5c\x99"
          "\x59\xd8\x37\x7a\xb6\x13\x29\x5d\x15\xe6\x66\x6b\xdc\x02\x34\x24"
          "\x6d\xee\x44\xbb\x17\x78\xee\xec\x40\x82\x16\x52\x31\x7a\x82\xe4"
          "\x36\x33\x41\x1d\x2d\xd6\x34\x08\x5a\x52\x24\x05\x0e\x87\x0c\xf7"
          "\x41\xac\x5c\xbf\xe2\x02\xec\x06\xc2\x1f\x5f\x7f\x2c\x7f\x8d\xde"
          "\x68\x00\xb2\x92\x45\x9d\xa1\x75\x91\x67\x86\x42\x58\x03\x31\x33"
          "\x32\xc9\xcb\x1b\x76\xa9\x44\x88\xf0\x3f\xc5\x51\x2c\x3e\x2f\x13");
        SB_1k(sb_encrypted);
        SB_1k(sb_clear);
        lstr_t text_fixed;

        /* We need a secret with fixed size 256: we use a truncated priv key
         * as data */
        assert (priv.len >= 256);
        text_fixed = LSTR_INIT_V(priv.s, 256);

        Z_ASSERT_P(ssl_ctx_init_pkey_pub(&ctx, pub));
        Z_ASSERT_GT(EVP_PKEY_CTX_set_rsa_padding(ctx.pkey_encrypt,
                                                 RSA_NO_PADDING), 0);
        Z_ASSERT_N(ssl_encrypt(&ctx, text_fixed, &sb_encrypted));
        Z_ASSERT_LSTREQUAL(text_encrypted, LSTR_SB_V(&sb_encrypted));
        ssl_ctx_wipe(&ctx);

        Z_ASSERT_P(ssl_ctx_init_pkey_priv(&ctx, priv, LSTR_EMPTY_V));
        Z_ASSERT_GT(EVP_PKEY_CTX_set_rsa_padding(ctx.pkey_decrypt,
                                                 RSA_NO_PADDING), 0);
        Z_ASSERT_N(ssl_decrypt(&ctx, LSTR_SB_V(&sb_encrypted), &sb_clear));
        Z_ASSERT_LSTREQUAL(text_fixed, LSTR_SB_V(&sb_clear));
        ssl_ctx_wipe(&ctx);
    } Z_TEST_END;

/* }}} */
/* {{{ PKEY */

    Z_TEST(pkey, "pkey") {
        ssl_ctx_t ctx;
        const lstr_t text_encrypted = LSTR_IMMED(
          "\x00\xee\x73\x51\xe4\xf7\x7d\x47\xd8\xd4\xbd\x4d\xfa\x61\xd9\xad"
          "\x71\xcc\x6e\x9f\x18\x99\xfe\xed\xfa\x8e\x68\x1a\xa2\x8b\xac\x54"
          "\xa4\xa0\x8a\x0c\x37\x3e\xfe\x6f\x49\xf3\xf5\xb0\x4f\x56\x14\xa1"
          "\x99\xb0\x7b\x6d\x29\x59\xd0\x43\x1b\x04\xef\x02\x7f\xea\x56\xe3"
          "\xb0\xf1\x36\x9b\x79\x9b\x44\x92\x39\xe9\x3c\xc6\xd5\x26\xb8\x39"
          "\x38\xd9\x7d\xaa\xa9\xa9\xda\xe6\x2b\x28\xa7\x0c\xa5\xf6\x70\x73"
          "\x10\x6c\x03\x5e\xc6\x6a\x84\x4d\x29\x76\x27\x2e\xc4\x0c\x8f\xae"
          "\x82\x2c\xd5\x41\x81\x3e\xf1\xc4\xcf\x1f\x3c\x04\x9f\x07\xd5\xed"
          "\xf9\x43\xe9\x42\x15\x0c\xa2\x63\xc9\x95\x7e\x78\xd4\x7b\x1b\x5f"
          "\xc2\xa1\xdd\xeb\xad\x1c\x1e\xf7\xeb\xee\xd8\x5b\xa8\x26\x7e\x8d"
          "\x72\x0c\x54\x83\xb1\xa3\xc8\x7e\xb9\x67\x61\x43\x40\xd0\x44\x30"
          "\x0e\x8e\xc3\x7e\xd0\x55\x11\x08\x44\x59\xac\x2a\x47\xa6\x45\xf0"
          "\x45\x43\xc5\x1e\xc9\xb6\xac\x25\xf7\x70\x76\x26\x70\x05\x6a\x5c"
          "\x55\xf5\x9a\x87\x51\xd2\xd2\x99\x4f\x73\x21\xe5\xf5\xad\x33\xa0"
          "\x26\xc9\x9f\xf2\xbc\x07\x8c\x5c\xe9\x45\x6c\x44\x7e\xc2\xa5\xa1"
          "\x15\x1a\x9c\x43\x04\x6e\x98\xaa\x34\xf5\x9c\x08\x83\x13\x82\x05");
        SB_1k(sb_encrypted);
        SB_1k(sb_clear);

        Z_ASSERT_NULL(ssl_ctx_init_pkey_pub(&ctx, LSTR_EMPTY_V));
        Z_ASSERT_NULL(ssl_ctx_init_pkey_priv(&ctx, LSTR_EMPTY_V,
                                             LSTR_EMPTY_V));
        Z_ASSERT_P(ssl_ctx_init_pkey_pub(&ctx, pub));
        Z_ASSERT_N(ssl_encrypt(&ctx, text, &sb_encrypted));
        Z_ASSERT_NEG(ssl_decrypt(&ctx, LSTR_SB_V(&sb_encrypted), &sb_clear));
        ssl_ctx_wipe(&ctx);

        Z_ASSERT_P(ssl_ctx_init_pkey_priv(&ctx, priv, LSTR_EMPTY_V));
        Z_ASSERT_N(ssl_decrypt(&ctx, LSTR_SB_V(&sb_encrypted), &sb_clear));
        Z_ASSERT_LSTREQUAL(text, LSTR_SB_V(&sb_clear));
        sb_reset(&sb_clear);

        Z_ASSERT_N(ssl_decrypt(&ctx, text_encrypted, &sb_clear));
        Z_ASSERT_LSTREQUAL(text, LSTR_SB_V(&sb_clear));
        sb_reset(&sb_clear);

        Z_ASSERT_NEG(ssl_decrypt(&ctx, text_garbage, &sb_clear));
        ssl_ctx_wipe(&ctx);

        sb_reset(&sb_encrypted);
        sb_reset(&sb_clear);
        Z_ASSERT_P(ssl_ctx_init_pkey_priv(&ctx, priv, LSTR_EMPTY_V));
        Z_ASSERT_N(ssl_encrypt(&ctx, text, &sb_encrypted));
        Z_ASSERT_N(ssl_decrypt(&ctx, LSTR_SB_V(&sb_encrypted), &sb_clear));
        Z_ASSERT_LSTREQUAL(text, LSTR_SB_V(&sb_clear));
        ssl_ctx_wipe(&ctx);
    } Z_TEST_END;

/* }}} */
/* {{{ PKEY for RSA with OAEP */

    Z_TEST(pkey_rsa_with_oeap, "pkey_rsa_with_oeap") {
        ssl_ctx_t ctx;
        /* same but encrypt with -pkeyopt rsa_padding_mode:oeap */
        const lstr_t text_encrypted = LSTR_IMMED(
          "\x18\x55\xef\x38\xf8\x54\x18\x13\x2e\xc1\x10\x43\xbf\xeb\x53\xb7"
          "\xeb\x6a\x6a\x8c\x26\xe8\xac\x1f\x09\xc5\x26\xfc\x6c\x15\x8d\x39"
          "\xc4\xd8\xc4\xf5\x55\x83\x02\xdb\xf7\x05\xe1\x3e\x0b\x9e\x44\xc8"
          "\x95\x9e\x4b\x86\x27\xfc\x3f\x8e\x2d\x6d\x6d\xed\x94\x51\x47\x56"
          "\x48\xff\x35\x3e\x11\x91\xf7\x57\x0d\xef\xce\xb4\x3e\x2d\xf7\x6f"
          "\x08\x44\x92\xa3\xcd\x13\xc4\xb7\x09\x9b\x07\x95\x69\xf6\x3a\x0c"
          "\x2b\x5b\x78\x9a\xe4\x3e\xe4\x52\x66\xd5\x71\xe2\xd9\x70\xdb\x78"
          "\x21\xa0\xfe\x18\xce\x7b\x74\x34\x71\xc0\x46\x73\xcd\x27\x8d\xe7"
          "\x5d\xd0\xee\x9c\xc0\x50\x39\x2c\x50\x3b\x67\x14\x56\xdd\x73\xa5"
          "\x12\xd5\x47\x59\x00\x94\x34\xcf\xb3\x6a\x45\x27\x19\x0e\x4c\x06"
          "\x37\xd1\xcc\xf0\x2c\x4c\x4d\x54\x8a\x88\x57\xe7\x9a\xcd\x06\x3a"
          "\x26\x31\xc7\xc9\x24\x4c\x68\x80\xd7\xaf\x69\xcf\x03\x4b\x46\xa9"
          "\x53\x0f\x70\x6b\xd5\x9d\x0e\x3b\x70\xa8\x45\xb8\x37\x8c\x2a\x6f"
          "\x6e\xb7\xc5\x5d\xf0\x4a\x79\xf4\x67\x66\x0e\x4f\xf7\x56\xb0\x71"
          "\x89\xbc\x31\x38\xa2\xa2\x18\x66\xd5\xc8\x52\x21\xb2\xca\x73\x0b"
          "\xa8\x4c\xab\xe6\x16\x4d\x2d\xd7\x83\xa5\xd3\x07\x83\xa2\x0c\x38");
        SB_1k(sb_encrypted);
        SB_1k(sb_clear);

        Z_ASSERT_P(ssl_ctx_init_pkey_pub(&ctx, pub));
        Z_ASSERT_GT(EVP_PKEY_CTX_set_rsa_padding(ctx.pkey_encrypt,
                                                 RSA_PKCS1_OAEP_PADDING), 0);
        Z_ASSERT_N(ssl_encrypt(&ctx, text, &sb_encrypted));
        ssl_ctx_wipe(&ctx);

        Z_ASSERT_P(ssl_ctx_init_pkey_priv(&ctx, priv, LSTR_EMPTY_V));
        Z_ASSERT_GT(EVP_PKEY_CTX_set_rsa_padding(ctx.pkey_decrypt,
                                                 RSA_PKCS1_OAEP_PADDING), 0);
        Z_ASSERT_N(ssl_decrypt(&ctx, LSTR_SB_V(&sb_encrypted), &sb_clear));
        Z_ASSERT_LSTREQUAL(text, LSTR_SB_V(&sb_clear));
        sb_reset(&sb_clear);

        Z_ASSERT_N(ssl_decrypt(&ctx, text_encrypted, &sb_clear));
        Z_ASSERT_LSTREQUAL(text, LSTR_SB_V(&sb_clear));
        ssl_ctx_wipe(&ctx);
    } Z_TEST_END;

/* }}} */
/* {{{ PKEY with passphrase */

    Z_TEST(pkey_with_passphrase, "pkey_with_passphrase") {
        ssl_ctx_t ctx;
        /* same but genpkey with -pass:this-is-a-complicated-pass-phrase */
        const lstr_t priv_with_pass  = LSTR_IMMED(
          "-----BEGIN PRIVATE KEY-----\n"
          "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCjamGL98i7M/WD\n"
          "Bvbr7wXoQYyjoCV/cTnbNKZNlJBMZ5qzYzSa5ZIdznzgfxjytXNPZ0egcqotDPye\n"
          "QAB9f3URJ0cz9yuUwxxg2y0yQWJQcPlkyTAC5stxuMfHHMVforyry47LtYlGYnCk\n"
          "X+1R8h5gqylb9vcVoTgO9195sEM6jrPniG1myFeWnpTaIxb1Of4z1HntRCNlfKb6\n"
          "z0FKOAYM7iDhFx0DeH6plrngRihGLxWxWH7RxuLDEJtrv0fVb54pJCKeSnSmoz5W\n"
          "W264jrKwuPRMdAE5rBbGFUc339XIcO2aL9P+dE9tqjxD2Oqq+48n+8P0Y+ikM2WW\n"
          "J40L1xcnAgMBAAECggEBAIec+fTBHckVVpJ1Dic/xgQ3mbIUben0GdJrP/Oz7Ygq\n"
          "lnx2QKqnB3pK6OEZOKf6owXrLMrfPZCDbYUakg2T35/rm7BpV7ZtsLhES56gGimt\n"
          "h5n3SCuwQndOpCP+IWG7WJ2tIQS204Qgn2AZ54WQy1rn0DvsmKJPl4j8CzSebTxC\n"
          "6uNOr8djWoAtyfN1M0R5d5nGF1qNRlsbd8fSduUn+3knvmPyLWzP2SVeQv/9M8eP\n"
          "Q4+3jR3mYucrIhLgAHQVhNzHhzgKc0o0XpMjTBXq20k4+sfVrU/npDBuYHgWribT\n"
          "ytG7dJ5cW/ggCkFgY4m8kRVOhswQQZRIww8ZF69XVAkCgYEA13h5E63QF35RWCUe\n"
          "1PNSeM+3x2HoQXehZ7jlpYzEj/xi8jWYGDI4ObotCyYtb73c0vuXYP8Mx2bGQTyQ\n"
          "pI0pvHG/QtMi4LaoMekXpi62ArSExdJvlC+8G2jbQlPfRBrtGoSXZ2SSysINV4Un\n"
          "Bq0gU8+QWdoCqK/F4ex6/A8joC0CgYEAwidO7+FjX3qPInbg863+amP6H7YBkwdK\n"
          "ComhzeZSkgOhn2xK4dvGTmP8EZd8KE5kWVsAsxSJuXddbu/9gSkrUMuvIcdHD74k\n"
          "+/ucKAdMQL+xqmdRo5S0dhqPAmEt/sipF3JLPeTyyWUk62dCWo+ABmWYqZVruX8g\n"
          "qBwcMa12lSMCgYEAifssDd4Qk/rgPIII7HWlKphaJ+Qax1HEmpdc+FbcyRfmhRSt\n"
          "AVGnj9AZaDpafmQnNTTIC+VIWakG7F/MgJOlVnfA8xoiC6TssImEC3d+Nt1C6SuJ\n"
          "KGwpGaRcRG1RXFuh2oluK1fMaOs7gABUrYHQYdtZpTBm438sSTEW0LMhLUECgYAg\n"
          "pxfBhDiAQE5+T1v535NgNTxFxQhyv9EWAJuz0z6jy/SMqVvWrG2nlW05UC7TYIvD\n"
          "82gkthmLlaWjGL2b0V61kev8VFWBMktqDaDvonqSkSrCK+oxBrtq+YB/t/RSW1EE\n"
          "3nYFDNJASMByzsT8EhJIASIxsy2Q3u6RF1kuiavd3QKBgGz1vuYpA3XhRyJx1yso\n"
          "P90D4aUZ4ohf16QvO6dKZgKqiwZouaBpv1tRhT0eDlxwCQM8K2g88V/xryXkXtfC\n"
          "E17Do7mq0S4BMCCCBoY5RqA/OuBeI+zNDdpA9ALjUS/YktIwbRa4JznsijDNq/Fa\n"
          "9T68azojUwhORQWsfP+KKqfv\n"
          "-----END PRIVATE KEY-----\n");
        const lstr_t pub_with_pass  = LSTR_IMMED(
          "-----BEGIN PUBLIC KEY-----\n"
          "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAo2phi/fIuzP1gwb26+8F\n"
          "6EGMo6Alf3E52zSmTZSQTGeas2M0muWSHc584H8Y8rVzT2dHoHKqLQz8nkAAfX91\n"
          "ESdHM/crlMMcYNstMkFiUHD5ZMkwAubLcbjHxxzFX6K8q8uOy7WJRmJwpF/tUfIe\n"
          "YKspW/b3FaE4DvdfebBDOo6z54htZshXlp6U2iMW9Tn+M9R57UQjZXym+s9BSjgG\n"
          "DO4g4RcdA3h+qZa54EYoRi8VsVh+0cbiwxCba79H1W+eKSQinkp0pqM+VltuuI6y\n"
          "sLj0THQBOawWxhVHN9/VyHDtmi/T/nRPbao8Q9jqqvuPJ/vD9GPopDNllieNC9cX\n"
          "JwIDAQAB\n"
          "-----END PUBLIC KEY-----\n");
        const lstr_t pass = LSTR_IMMED("this-is-a-complicated-pass-phrase");
        const lstr_t text_encrypted = LSTR_IMMED(
          "\x87\xf9\xa5\x86\x33\xc8\x04\x87\xbb\xc2\x82\x91\x29\x30\x98\x8e"
          "\x8e\x44\x06\x2a\xa4\x43\x2c\x7e\x63\x80\x3d\x78\x6b\xa6\x21\x7e"
          "\x57\xe2\xe7\x4d\xe5\x5f\x11\x1e\xa5\x4b\x33\x5d\xb3\x97\x42\x57"
          "\xcb\xdb\x4b\x45\x1a\xcc\x3f\x95\xf3\x56\xa3\xb6\x2a\x06\xce\x5b"
          "\x25\xd2\xae\xaf\xdc\xa6\xe6\xba\x81\x49\xf0\x9c\x13\x5e\x10\x6d"
          "\x54\xfe\xbc\x75\xae\x8b\x82\x45\x7a\x8f\x4e\x4d\x08\x30\x12\x17"
          "\xb9\x17\x55\xc8\x88\x65\x29\x35\x02\x40\x64\x35\x48\xdb\x12\x88"
          "\x2a\xda\x80\x68\x9d\x95\x61\xc6\x7f\x0c\x78\xe2\x2a\xf7\x12\x98"
          "\x7d\x25\x4e\xa2\xcb\x3d\x3a\xf6\x4e\x30\xfd\xb3\x81\x5a\xec\xf7"
          "\x4c\x9e\x76\xe2\xa6\x16\x22\x9e\x16\xb8\x1e\xb0\xa1\xd9\x91\x76"
          "\xfb\x4d\xf8\x9a\xbc\xc4\xaa\xfd\x3d\x15\xaf\xb5\x64\x4e\x69\x9b"
          "\x14\x7c\x18\xbe\xe7\xb5\x14\xca\xad\x4c\xbc\xb7\xac\x02\x30\xff"
          "\x71\xd6\x51\xff\x79\xdd\x6e\xca\x59\xac\x72\x34\x37\x49\x5e\x5e"
          "\x47\x56\xe4\xb5\x8c\xb6\x8b\xdc\x78\xc0\x71\xe7\xf6\x13\xf0\x02"
          "\xff\x18\x62\xdb\x24\x0e\xbe\xd4\xdf\x27\xfb\xa7\x85\x1b\xfc\xbc"
          "\x67\xe8\xc7\xd7\xa0\xf8\xfb\x51\x75\x29\x8d\x56\x2d\xaa\xb0\x60");
        SB_1k(sb_encrypted);
        SB_1k(sb_clear);

        Z_ASSERT_P(ssl_ctx_init_pkey_pub(&ctx, pub_with_pass));
        Z_ASSERT_N(ssl_encrypt(&ctx, text, &sb_encrypted));
        ssl_ctx_wipe(&ctx);

        Z_ASSERT_P(ssl_ctx_init_pkey_priv(&ctx, priv_with_pass, pass));
        Z_ASSERT_N(ssl_decrypt(&ctx, LSTR_SB_V(&sb_encrypted), &sb_clear));
        Z_ASSERT_LSTREQUAL(text, LSTR_SB_V(&sb_clear));
        sb_reset(&sb_clear);

        Z_ASSERT_N(ssl_decrypt(&ctx, text_encrypted, &sb_clear));
        Z_ASSERT_LSTREQUAL(text, LSTR_SB_V(&sb_clear));
        ssl_ctx_wipe(&ctx);
    } Z_TEST_END;

/* }}} */

    }
#endif

/* }}} */
/* {{{ licence_encryption_key */

    Z_TEST(licence_encryption_key, "") {
        conf_t *conf;
        const char *signature;
        char *encrypted_key;
        SB_1k(sb);

        Z_ASSERT_N(chdir(z_cmddir_g.s));

        Z_ASSERT(conf = conf_load("samples/licence-v1-ok.conf"));
        Z_ASSERT(licence_check_signature_ok(conf));

        /* Test encryption key computation */
        Z_ASSERT_P(signature = conf_get_raw(conf, "licence", "signature"));
        Z_ASSERT_P(encrypted_key = licence_compute_encryption_key(signature, "plop"));
        Z_ASSERT(conf_put(conf, "licence", "encryptionKey", encrypted_key));

        /* Test encryption key resolver */
        Z_ASSERT_N(licence_resolve_encryption_key(conf, &sb));
        Z_ASSERT_STREQUAL("plop", sb.data);

        conf_delete(&conf);
        p_delete(&encrypted_key);
    } Z_TEST_END;

/* }}} */

    MODULE_RELEASE(ssl);

} Z_GROUP_END

/* }}} */
