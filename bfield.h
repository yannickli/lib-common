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

#ifndef IS_LIB_COMMON_BFIELD_H
#define IS_LIB_COMMON_BFIELD_H

#include "core.h"

typedef struct bfield_t {
    ssize_t offs;
    sb_t    bits;
} bfield_t;

static inline bfield_t *bfield_init(bfield_t *bf) {
    bf->offs = 0;
    sb_init(&bf->bits);
    return bf;
}
static inline void bfield_wipe(bfield_t *bf) {
    sb_wipe(&bf->bits);
}
GENERIC_NEW(bfield_t, bfield);
GENERIC_DELETE(bfield_t, bfield);

void bfield_set(bfield_t *bf, ssize_t pos);
void bfield_unset(bfield_t *bf, ssize_t pos);
bool bfield_isset(const bfield_t *bf, ssize_t pos);
int  bfield_count(const bfield_t *bf);

static inline void bfield_purge(bfield_t *bf) {
    bfield_wipe(bf);
    bfield_init(bf);
}

#endif
