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
#ifndef IS_LIB_COMMON_DOMAINS_LIST_H
#define IS_LIB_COMMON_DOMAINS_LIST_H

#include "macros.h"

typedef struct domain_index_t {
    struct node_t *nodes;
    int freenode;
    int nbnodes;
} domains_index_t;

domains_index_t *domains_index_new(void);
void domains_index_delete(domains_index_t **idx);

int domains_index_add(domains_index_t *idx, const char *key, int value);
int domains_index_get(const domains_index_t *idx, const char *key, int trace);
#endif /* IS_LIB_COMMON_DOMAINS_LIST_H */
