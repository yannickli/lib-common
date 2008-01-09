/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_ROBUF_H
#define IS_LIB_COMMON_ROBUF_H

#include "macros.h"
#include "mem.h"

/* This is a "read-only" buffer : The function that builds it has the
 * responsability of choosing the most efficient solution (memory/ease
 * of use). Then, the user gets two fields : (data,size), and has the
 * responsability of calling robuf_free() when done.
 */
typedef enum robuf_type {
    ROBUF_TYPE_NONE,
    ROBUF_TYPE_BLOB,
    ROBUF_TYPE_CONST,
    ROBUF_TYPE_MALLOC,
    //ROBUF_TYPE_MMAP, could be usefull...
} robuf_type;

typedef struct robuf {
    /* Public fields, read-only */
    const byte *data;
    int size;

    /* Private fields */
    robuf_type type;
    blob_t blob;
} robuf;

robuf *robuf_init(robuf *rob);
void robuf_wipe(robuf *rob);
void robuf_reset(robuf *rob);
/* OG: should be robuf_init_xxx */
void robuf_make_const(robuf *dst, const byte *data, int size);
void robuf_make_malloced(robuf *dst, const byte *data, int size);
blob_t *robuf_make_blob(robuf *dst);
void robuf_blob_consolidate(robuf *dst);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_robuf_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_ROBUF_H */
