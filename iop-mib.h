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

typedef struct mib_revision_t {
    lstr_t timestamp;
    lstr_t description;
} mib_revision_t;

qvector_t(mib_rev, mib_revision_t);
qvector_t(pkg, iop_pkg_t *);

/* }}} */

/** Write the MIB file corresponding to IOP packages.
 *
 * \param[in]  argc        Number of received arguments.
 * \param[in]  argv        Arguments received.
 * \param[in]  pkgs        List of the different iop packages that will be
 *                         added to the MIB.
 * \param[in]  revisions   List of the different revisions the MIB has had,
 *                         \note: the order of the different elements of the
 *                         qv must follow the chronological order, from the
 *                         initial to the last revision.
 */
int iop_mib(int argc, char **argv, qv_t(pkg) pkgs, qv_t(mib_rev) revisions);

/** Register a package into a qv of packages.
 *
 * \param[out]  _vec  The qv_t(pkg) of packages to register the package into.
 * \param[in]   _pkg  Name of the package.
 */
#define mib_register_pkg(_vec, _pkg)  \
    qv_append(pkg, _vec, (void *)&_pkg##__pkg)

/** Register a revision into a qv of revisions.
 *
 * \param[out]  _vec          The qv_t(revi) of revisions to register the
 *                            revision into.
 * \param[in]   _timestamp    Timestamp of the new revision.
 * \param[in]   _description  Description of the new revision.
 */
#define mib_register_revision(_vec, _timestamp, _description)  \
    ({                                                                       \
        mib_revision_t _rev = {                                              \
            .timestamp = LSTR(_timestamp),                                   \
            .description = LSTR(_description),                               \
        };                                                                   \
        qv_append(mib_rev, _vec, _rev);                                      \
    })

#endif /* IS_LIB_COMMON_IOP_MIB_H */
