/***************************************************************************/
/*                                                                         */
/* Copyright 2019 INTERSEC SA                                              */
/*                                                                         */
/* Licensed under the Apache License, Version 2.0 (the "License");         */
/* you may not use this file except in compliance with the License.        */
/* You may obtain a copy of the License at                                 */
/*                                                                         */
/*     http://www.apache.org/licenses/LICENSE-2.0                          */
/*                                                                         */
/* Unless required by applicable law or agreed to in writing, software     */
/* distributed under the License is distributed on an "AS IS" BASIS,       */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/* See the License for the specific language governing permissions and     */
/* limitations under the License.                                          */
/*                                                                         */
/***************************************************************************/

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
    lstr_t expected;
    be32_t salt;

#ifndef NDEBUG
    if (lstr_equal(sig, LSTR("$42:defeca7e$")))
        return 0;
#endif

    if (F(iop_signature_get_salt)(sig, &salt) < 0) {
        return -1;
    }

    expected = F(t_iop_sign_salt_sha256)(st, v, be_to_cpu32(salt), flags);
    if (!lstr_equal(sig, expected)) {
        if (!(flags & IOP_HASH_SKIP_DEFAULT)) {
            return -1;
        }

        expected = F(t_iop_sign_salt_sha256)(st, v, be_to_cpu32(salt),
                                        flags | IOP_HASH_SHALLOW_DEFAULT);
        if (!lstr_equal(sig, expected)) {
            return -1;
        }
    }

    return 0;
}
