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

#include <openssl/err.h>
#include <lib-common/z.h>
#include "ssl.h"

/* {{{ Initialize & wipe */

static __thread char openssl_errbuf_g[1024];

#define ssl_set_error(msg) \
    ({ snprintf(openssl_errbuf_g, sizeof(openssl_errbuf_g),            \
                msg ": %s", ERR_error_string(ERR_get_error(), NULL));  \
       -1; })


static ssl_ctx_t *ssl_ctx_init(ssl_ctx_t *ctx)
{
    p_clear(ctx, 1);

    ctx->encrypt = EVP_CIPHER_CTX_new();
    ctx->decrypt = EVP_CIPHER_CTX_new();

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

    EVP_CIPHER_CTX_free(ctx->encrypt);
    EVP_CIPHER_CTX_free(ctx->decrypt);
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
        if (unlikely(!EVP_EncryptInit_ex(ctx->encrypt, ctx->type, NULL,
                                         keybuf, ivbuf)))
        {
            return ssl_set_error("encrypt context initialization failure");
        }
        ctx->encrypt_state = SSL_CTX_INIT;
    }

    if (do_decrypt) {
        if (unlikely(!EVP_DecryptInit_ex(ctx->decrypt, ctx->type, NULL,
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
/* {{{ Encrypt */

int ssl_encrypt_update(ssl_ctx_t *ctx, lstr_t data, sb_t *out)
{
    int clen = data.len + EVP_CIPHER_CTX_block_size(ctx->encrypt) + 1;

    assert (ctx->encrypt_state == SSL_CTX_INIT
        ||  ctx->encrypt_state == SSL_CTX_UPDATE);

    if (unlikely(data.len <= 0)) {
        /* XXX with openssl < 1.0, EVP_EncryptUpdate will abort() with empty
         * data. */
        ctx->encrypt_state = SSL_CTX_UPDATE;
        return 0;
    }

    if (unlikely(!EVP_EncryptUpdate(ctx->encrypt,
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

    if (unlikely(!EVP_EncryptFinal_ex(ctx->encrypt,
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
        if (unlikely(!EVP_EncryptInit_ex(ctx->encrypt, NULL, NULL, NULL,
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
    int clen = data.len + EVP_CIPHER_CTX_block_size(ctx->decrypt) + 1;

    assert (ctx->decrypt_state == SSL_CTX_INIT
        ||  ctx->decrypt_state == SSL_CTX_UPDATE);

    if (unlikely(!EVP_DecryptUpdate(ctx->decrypt,
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

    if (unlikely(!EVP_DecryptFinal_ex(ctx->decrypt,
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
        if (unlikely(!EVP_DecryptInit_ex(ctx->decrypt, NULL, NULL, NULL,
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
    Z_TEST(encrypt, "encrypt") {
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

    Z_TEST(decrypt, "decrypt") {
        ssl_ctx_t ctx;
        const lstr_t text = LSTR_IMMED("\x12\x29\xB4\x7E\x75\xE1\x33\x64"
                                       "\x49\x09\x77\xF8\xE8\x08\x2C\x58");
        SB_1k(sb);

        Z_ASSERT_P(ssl_ctx_init_aes256(&ctx, LSTR("plop"), 42, 1024));

        Z_ASSERT_N(ssl_decrypt(&ctx, text, &sb));

        Z_ASSERT_LSTREQUAL(LSTR("Encrypt me"), LSTR_SB_V(&sb));

        ssl_ctx_wipe(&ctx);
    } Z_TEST_END;

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
} Z_GROUP_END

/* }}} */
