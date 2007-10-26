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

#ifndef IS_LIB_COMMON_HASHTBL_H
#define IS_LIB_COMMON_HASHTBL_H

#include "mem.h"

typedef struct hashtbl_t {
    ssize_t nr, size;
    struct hashtbl_entry *tab;
} hashtbl_t;

GENERIC_INIT(hashtbl_t, hashtbl);
void hashtbl_wipe(hashtbl_t *t);

void **hashtbl_find(const hashtbl_t *t, uint64_t h);
void **hashtbl_insert(hashtbl_t *t, uint64_t h, void *);
void hashtbl_remove(hashtbl_t *t, void **);
void hashtbl_map(hashtbl_t *t, void (*fn)(void **, void *), void *);

#endif /* IS_LIB_COMMON_HASHTBL_H */
