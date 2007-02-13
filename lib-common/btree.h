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

#ifndef IS_LIB_COMMON_BTREE_H
#define IS_LIB_COMMON_BTREE_H

#include "blob.h"
#include "mmappedfile.h"

typedef struct btree_priv_t btree_priv_t;
typedef struct btree_t MMFILE_ALIAS(struct btree_priv) btree_t;

int btree_fsck(btree_t *bt, int dofix);
btree_t *btree_open(const char *path, int flags);
btree_t *btree_creat(const char *path);
void btree_close(btree_t **tree);

int btree_fetch(const btree_t *bt, const byte *key, int klen, blob_t *out);
int btree_push(btree_t *bt, const byte *key, int klen,
               const byte *data, int len);

void btree_dump(FILE *out, const btree_t *bt,
                void (*k_fmt)(FILE *, const byte *, int),
                void (*d_fmt)(FILE *, const byte *, int));

#endif
