/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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
    LICENCE_INVALID_EXPIRATION = -2,
    LICENCE_HARD_EXPIRED = -1,

    /* Warning cases */
    LICENCE_SOFT_EXPIRED = 1,
    LICENCE_EXPIRES_SOON = 2,
} licence_expiry_t;

/** Check an IOP Licence.
 *
 * \warning this function won't check the licence expiry: use
 *          \ref licence_check_iop_expiry for that.
 *
 * \param[in] licence     The signed licence structure.
 * \param[in] version     The version of the product we're running on,
 *                        LSTR_NULL if the version should not be checked.
 * \param[in] flags       Flags to use to compute the signature.
 */
__must_check__
int licence_check_iop(const struct core__signed_licence__t *licence,
                      lstr_t version, unsigned flags);

/** Check the expiration of an IOP Licence.
 *
 * \param[in]  licence      The licence structure to check.
 */
__must_check__ licence_expiry_t
licence_check_iop_expiry(const struct core__licence__t *licence);

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
/*{{{ Private function. Exposed for unit tests pruposes only. */

__must_check__
int licence_check_modules(const struct core__licence__t *licence);

/*}}} */
#endif /* IS_LIB_COMMON_LICENCE_H */
