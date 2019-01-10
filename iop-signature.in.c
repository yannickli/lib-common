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

/* XXX Make this file syntastic-compatible. */
#ifndef F
#  include "iop.h"
#  define F(x)  x
#endif

#ifndef ATTRS
#  define ATTRS
#endif

ATTRS
static lstr_t F(t_iop_sign_salt_sha256)(const iop_struct_t *st, const void *v,
                                        uint32_t salt, unsigned flags)
{
    uint8_t buf[SHA256_DIGEST_SIZE];
    be32_t  s = cpu_to_be32(salt);

    (iop_hmac_sha256)(st, v, LSTR_INIT_V((void *)&s, sizeof(s)), buf, flags);
    return t_lstr_fmt("$256:%*pX$%*pX", (int)sizeof(s), &s,
                      SHA256_DIGEST_SIZE, buf);
}

ATTRS
__must_check__
static int F(iop_signature_get_salt)(lstr_t signature, be32_t *salt)
{
    if (lstr_startswith(signature, LSTR("$256:"))) {
        if (signature.len != 5 + 8 + 1 + SHA256_DIGEST_SIZE * 2)
        {
#ifdef NDEBUG
            return -1;
#else
            return e_error("invalid $256 signature (invalid length)");
#endif
        }
        if (strconv_hexdecode(salt, sizeof(*salt), signature.s + 5, 8) < 0
        ||  signature.s[5 + 8] != '$')
        {
#ifdef NDEBUG
            return -1;
#else
            return e_error("invalid $256 signature (invalid salt)");
#endif
        }
    } else {
#ifdef NDEBUG
        return -1;
#else
        return e_error("unparseable signature: <%*pM>",
                       LSTR_FMT_ARG(signature));
#endif
    }
    return 0;
}

ATTRS
#ifdef ALL_STATIC
static
#endif
int F(iop_check_signature)(const iop_struct_t *st, const void *v, lstr_t sig,
                           unsigned flags)
{
    t_scope;
    lstr_t exp;
    be32_t salt;

#ifndef NDEBUG
    if (lstr_equal(sig, LSTR("$42:defeca7e$")))
        return 0;
#endif

    if (F(iop_signature_get_salt)(sig, &salt) < 0) {
        return -1;
    }

    exp = F(t_iop_sign_salt_sha256)(st, v, be_to_cpu32(salt), flags);
    if (!lstr_equal(sig, exp)) {
        if (!(flags & IOP_HASH_SKIP_DEFAULT)) {
            return -1;
        }

        exp = F(t_iop_sign_salt_sha256)(st, v, be_to_cpu32(salt),
                                        flags | IOP_HASH_SHALLOW_DEFAULT);
        if (!lstr_equal(sig, exp)) {
            return -1;
        }
    }

    return 0;
}
