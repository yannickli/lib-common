/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
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

#include "blob.h"

typedef struct bfield_t {
    ssize_t offs;
    blob_t bits;
} bfield_t;

static inline bfield_t *bfield_init(bfield_t *bf) {
    bf->offs = 0;
    blob_init(&bf->bits);
    return bf;
}
static inline void bfield_wipe(bfield_t *bf) {
    blob_wipe(&bf->bits);
}
GENERIC_NEW(bfield_t, bfield);
GENERIC_DELETE(bfield_t, bfield);

void bfield_set(bfield_t *bf, ssize_t pos);
void bfield_unset(bfield_t *bf, ssize_t pos);
bool bfield_isset(bfield_t *bf, ssize_t pos);


static inline void bfield_purge(bfield_t *bf) {
    bfield_wipe(bf);
    bfield_init(bf);
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_bfield_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_XML_H */
