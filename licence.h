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

#ifndef IS_LIB_COMMON_LICENCE_H
#define IS_LIB_COMMON_LICENCE_H

#include "iop.h"
#include "conf.h"

/* Conf Licences {{{ */

extern int trace_override;
#define trace_override  trace_override

bool is_my_mac_addr(const char *addr);
int  list_my_macs(char *dst, size_t size);
int  list_my_cpus(char *dst, size_t size);
int  read_cpu_signature(uint32_t *dst);
bool licence_check_expiration_ok(const conf_t *conf);
bool licence_check_signature_ok(const conf_t *conf);
bool licence_check_specific_host_ok(const conf_t *conf);
bool licence_check_general_ok(const conf_t *conf);
bool licence_check_host_ok(const conf_t *conf);
/* WARNING: dst must be at least 65 char long, but the compiler will
 * not enforce this check, no matter what the prototype looks like.
 */
int  licence_do_signature(const conf_t *conf, char dst[65]);

/* }}} */
/* IOP Licences {{{ */

struct core__signed_licence__t;
struct core__licence__t;
struct core__licence_module__t;

typedef enum licence_expiry_t {
    LICENCE_OK = 0,

    /* Error cases */
    LICENCE_TOKEN_EXPIRED = -3,
    LICENCE_INVALID_EXPIRATION = -2,
    LICENCE_HARD_EXPIRED = -1,

    /* Warning cases */
    LICENCE_SOFT_EXPIRED = 1,
    LICENCE_EXPIRES_SOON = 2,
    LICENCE_TOKEN_EXPIRES_SOON = 3,
} licence_expiry_t;

/** Check the licence is valid for the current host.
 *
 * \param[in]  licence      The licence structure to check.
 */
__must_check__
int licence_check_iop_host(const struct core__licence__t *licence);

/** Check the expiration of an IOP Licence.
 *
 * \param[in]  licence      The licence structure to check.
 */
__must_check__ licence_expiry_t
licence_check_iop_expiry(const struct core__signed_licence__t *licence);

/** Check the licence modules are valid.
 *
 * \param[in]  licence      The licence structure to check.
 */
__must_check__
int licence_check_modules(const struct core__licence__t *licence);

/** Returns a licence module.
 *
 * \param[in] licence   The licence from which we want a module.
 * \param[in] mod       The module we want.
 *
 * \return              The module if it exists, else NULL.
 */
__must_check__
const struct core__licence_module__t *
licence_get_module_desc(const struct core__licence__t *licence,
                        const iop_struct_t *mod);
#define licence_get_module(licence, pfx) \
    ((const pfx##__t*)licence_get_module_desc(licence, &pfx##__s))

/** Check whether a module is activated or not.
 *
 * \param[in]  mod      The module of which activation must be checked.
 * \param[in]  licence  The licence in which the check must be done.
 *
 * \return              true if it is activated, false if it is not.
 */
__must_check__
bool licence_is_module_activated_desc(const struct core__licence__t *licence,
                                      const iop_struct_t *mod);
#define licence_is_module_activated(licence, pfx) \
    licence_is_module_activated_desc(licence, &pfx##__s)

/** Check the expiration of a licence module in an IOP Licence.
 *
 * \param[in]  licence      The licence module structure to check.
 */
__must_check__ licence_expiry_t
licence_check_module_expiry(const struct core__licence_module__t *licence);

/* }}} */
/* {{{ Activation tokens */

/* An activation token is a string composed of ACTIVATION_TOKEN_LENGTH
 * base64 characters of the form [pe | te | hash | rest] where:
 *   - pe is the number of days after which the product expires,
 *     counting from the associated licence signature timestamp.
 *     It is two char length (which is sufficient for around 11
 *     years).
 *   - te is the number of days after which the token cannot be
 *     used.  It is also two char length.
 *   - hash is the base64 encoding of the crc64 of the associated
 *     licence, pe and te.
 *   - rest is noise.
 *
 * The values pe and te represents 3 octets, which are obfuscated by
 * xor-ing them with the 3 first octets of the rest (before they're
 * put as base64).
 *
 * Note: we just use a hash and not a signature because (i) the length
 * of the tokens are too small to allow real cryptography, and (ii)
 * signatures do not prevent the attacker to hack binaries. A simple
 * hash solution is considered proportional to the risks encountered.
 */
#define ACTIVATION_TOKEN_LENGTH  20

/** Parse an activation token.
 *
 * \param[in]  rawtoken  The activation token.
 * \param[in]  licence  The associated licence, used for hash computation.
 * \param[out]  product_exp  The product expiration date (as a timestamp).
 * \param[out]  token_exp  The token expiration date (as a timestamp).
 * \param[out]  token  The activation token as defined in an IOP.
 * \param[out]  err  A buffer for error messages.
 *
 * \return 0 on success, -1 on error, -2 if the hash is invalid. In
 * this later case, the other fields have been "sucessfully" parsed.
 */
int t_parse_activation_token(lstr_t rawtoken,
                             const core__licence__t * nonnull licence,
                             time_t * nullable product_exp,
                             time_t * nullable token_exp,
                             core__activation_token__t * nullable token,
                             sb_t * nullable err);

/** Format an activation token based on what it depends.
 *
 * Note that the timestamps provided are rounded to the lower day.
 *
 * \param[in]  licence  The licence associated with the token.
 * \param[in]  product_exp  The product will no longer be active after
 *                          this date.
 * \param[in]  token_exp  The token cannot be applied after this date.
 * \param[out]  token  The activation token, as a string.
 *
 * \return 0 on success, -1 on error.
 */
int t_format_activation_token(const core__signed_licence__t * nonnull licence,
                              time_t product_exp, time_t token_exp,
                              core__activation_token__t * nonnull token,
                              sb_t * nonnull err);

/* }}} */

#endif /* IS_LIB_COMMON_LICENCE_H */
