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

typedef blob_t bfield_t;

bfield_t *bfield_init(bfield_t *blob);
void bfield_wipe(bfield_t *blob);
GENERIC_NEW(bfield_t, bfield);
GENERIC_DELETE(bfield_t, bfield);

void bfield_set(bfield_t *blob, int pos);
void bfield_unset(bfield_t *blob, int pos);
bool bfield_isset(bfield_t *blob, int pos);

void bfield_reset(bfield_t *blob);

/**
 * Clear memory
 */
static inline void bfield_purge(bfield_t *blob)
{
    bfield_wipe(blob);
    bfield_init(blob);
}

void bfield_dump(bfield_t *blob, int level);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_bfield_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_XML_H */
