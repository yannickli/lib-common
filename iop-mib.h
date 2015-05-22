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

void iop_mib(sb_t *, qv_t(pkg), qv_t(revi));

#define mib_register(h, _pkg) \
    qv_append(pkg, h, (void *)&_pkg##__pkg)

#endif /* IS_LIB_COMMON_IOP_MIB_H */
