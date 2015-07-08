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

#ifndef IS_LIB_COMMON_IOP_SNMP_DOC_H
#define IS_LIB_COMMON_IOP_SNMP_DOC_H

#include "iopc/iopc.h"

qvector_t(pkg, iop_pkg_t *);

#define doc_register_pkg(_vec, _pkg)  \
    qv_append(pkg, _vec, (void *)&_pkg##__pkg)

/** Generate the DOC corresponding to IOP packages.
 *
 * \param[in]  pkgs        List of the different iop packages that will be
 *                         added to the MIB.
 */
int iop_snmp_doc(int argc, char **argv, qv_t(pkg) pkgs);

#endif /* IS_LIB_COMMON_IOP_SNMP_DOC_H */
