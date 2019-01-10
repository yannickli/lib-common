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

enum {
    /* XXX: must not conflict with IOP_HASH_SKIP_* enum */
    LICENCE_SKIP_VERSION = 1 << 31
};

/** Check an IOP Licence.
 *
 * \param[in] licence     The signed licence structure.
 * \param[in] licence_st  The class the licence is expected to be a child of
 *                        (should be a descendant of core__licence__t).
 * \param[in] version     The version of the product we're running on,
 *                        LSTR_NULL if the version should not be checked.
 * \param[in] flags       Flags to use to compute the signature.
 */
__must_check__
int licence_check_iop(const struct core__signed_licence__t *licence,
                      const iop_struct_t *licence_st, lstr_t version,
                      unsigned flags);

/** Check the expiration of an IOP Licence.
 *
 * \param[in] licence    The licence structure to check.
 * \param[in] reference  The reference time; for example, call with
 *                       lp_getsec() to check if the licence is expired, or
 *                       with lp_getsec() + 3600 to check if the licence will
 *                       be expired in one hour.
 */
__must_check__
int licence_check_iop_expiry(const struct core__licence__t *licence,
                             time_t reference);

/* }}} */

#endif /* IS_LIB_COMMON_LICENCE_H */
