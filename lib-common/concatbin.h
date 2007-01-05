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

#ifndef IS_LIB_COMMON_CONCATBIN_H
#define IS_LIB_COMMON_CONCATBIN_H

#include <stdlib.h>

#include "macros.h"

/* The format of the archive is very simple :
 *
 * The archive contains blobs of known length.
 * Each blob is described by its length (in bytes), printed in ASCII and
 * followed by a "\n" :
 * N "\n" [DATA1 DATA2 DATA3 ... DATA-N] 
 */
typedef struct concatbin {
    const byte *start;
    const byte *cur;
    ssize_t len;
} concatbin;

concatbin *concatbin_new(const char *filename);
int concatbin_getnext(concatbin *ccb, const byte **data, int *len);
void concatbin_delete(concatbin **ccb);

#endif /* IS_LIB_COMMON_CONCATBIN_H */
