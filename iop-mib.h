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

#ifndef IS_LIB_COMMON_IOP_MIB_H
#define IS_LIB_COMMON_IOP_MIB_H

#include "iopc/iopc.h"

/* {{{ Local type definitions */

typedef struct revision_t {
    lstr_t timestamp;
    lstr_t description;
} revision_t;

GENERIC_NEW_INIT(revision_t, revision);
static inline void revision_wipe(revision_t *rev)
{
    lstr_wipe(&rev->timestamp);
    lstr_wipe(&rev->description);
}
GENERIC_DELETE(revision_t, revision);

qvector_t(revi, revision_t *);
qvector_t(pkg, iop_pkg_t *);

/* }}} */

/** Generate the MIB corresponding to IOP packages.
 *
 * \param[out] sb          Output buffer.
 * \param[in]  pkgs        List of the different iop packages that will be
 *                         added to the MIB.
 * \param[in]  revisions   List of the different revisions the MIB has had,
 *                         \note: the order of the different elements of the
 *                         qv must follow the chronological order, from the
 *                         initial to the last revision.
 */
void iop_mib(sb_t *sb, qv_t(pkg) pkgs, qv_t(revi) revisions);

#define mib_register(h, _pkg)  \
    qv_append(pkg, h, (void *)&_pkg##__pkg)

#endif /* IS_LIB_COMMON_IOP_MIB_H */
