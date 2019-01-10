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

#ifndef IS_LIB_COMMON_IOP_SNMP_H
#define IS_LIB_COMMON_IOP_SNMP_H

#include "iop.h"

/* {{{ Local type definitions */

typedef struct mib_revision_t {
    lstr_t timestamp;
    lstr_t description;
} mib_revision_t;

qvector_t(mib_rev, mib_revision_t);
qvector_t(pkg, const iop_pkg_t *);

/* }}} */
/* {{{ MIB generation API */

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

/** Generate a MIB into a sb_t.
 *
 * \param[out] sb  The string buffer in which the MIB will be generated.
 * \param[in]  pkgs        List of the different iop packages that will be
 *                         added to the MIB.
 * \param[in]  revisions   List of the different revisions the MIB has had,
 *                         \note: the order of the different elements of the
 *                         qv must follow the chronological order, from the
 *                         initial to the last revision.
 */
void iop_write_mib(sb_t *sb, const qv_t(pkg) *pkgs,
                   const qv_t(mib_rev) *revisions);

/** Run a MIB generation tool.
 *
 * Call this function to implement a MIB generation tool, which will dump the
 * MIB on stdout or in a file, depending on the given command-line arguments.
 *
 * \param[in]  argc        Number of received arguments.
 * \param[in]  argv        Arguments received.
 * \param[in]  pkgs        \ref iop_write_mib.
 * \param[in]  revisions   \ref iop_write_mib.
 */
int iop_mib(int argc, char **argv, const qv_t(pkg) *pkgs,
            const qv_t(mib_rev) *revisions);

/* }}} */
/* {{{ SNMP doc generation API */

/** Generate a SNMP doc into string buffers.
 *
 * \param[out] notif_sb   The output buffer for notifications doc.
 * \param[out] object_sb  The output buffer for objects doc.
 * \param[in]  pkgs       List of the different iop packages that will be
 *                        added to the documentation.
 */
void iop_write_snmp_doc(sb_t *notif_sb, sb_t *object_sb,
                        const qv_t(pkg) *pkgs);

/** Run a SNMP doc generation tool.
 *
 * Call this function to implement a SNMP doc generation tool, which will dump
 * the adoc documentation into files, parsing the command-line arguments.
 *
 * \param[in]  argc        Number of received arguments.
 * \param[in]  argv        Arguments received.
 * \param[in]  pkgs        \ref iop_write_snmp_doc.
 */
int iop_snmp_doc(int argc, char **argv, const qv_t(pkg) *pkgs);

/* }}} */

#endif /* IS_LIB_COMMON_IOP_SNMP_H */
