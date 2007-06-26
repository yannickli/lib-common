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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "blob.h"
#include "mmappedfile.h"

/* Kludge for passing extra open options */
#define BT_O_NOCHECK  O_NONBLOCK

typedef struct btree_priv_t btree_priv_t;
typedef struct btree_t MMFILE_ALIAS(struct btree_priv) btree_t;

typedef int btree_print_fun(FILE *fp, const char *fmt, ...)
	__attr_printf__(2, 3);

int btree_check_integrity(btree_t *bt, int dofix,
			  btree_print_fun *fun, FILE *arg);
btree_t *btree_open(const char *path, int flags);
btree_t *btree_creat(const char *path);
void btree_close(btree_t **tree);

/* OG: Should have both APIs, the default taking a (byte *, len) couple.
 * these should be called btree_fetch_uint64 and btree_push_uint64.
 * Actually, since the implementation seems hardwired for uint64_t
 * keys, the module itself and function/type prefix should be btree64.
 * Constness of the btree_t is not required either, and may not be
 * advisable, as per our conversation.
 */
int btree_fetch(const btree_t *bt, uint64_t key, blob_t *out);
/* OG: data argument should be const void * */
int btree_push(btree_t *bt, uint64_t key, const byte *data, int len);

void btree_dump(const btree_t *bt, btree_print_fun *fun, FILE *arg);

typedef struct fbtree_t fbtree_t;

fbtree_t *fbtree_open(const char *path);
int fbtree_fetch(fbtree_t *, uint64_t key, blob_t *out);
void fbtree_close(fbtree_t **f);

#endif
