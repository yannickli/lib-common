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

#ifndef IS_LIB_COMMON_BTREE_H
#define IS_LIB_COMMON_BTREE_H

#include "mmappedfile.h"

typedef struct btree_priv_t btree_priv_t;
typedef struct btree_t MMFILE_ALIAS(struct btree_priv) btree_t;

typedef int btree_print_fun(FILE *fp, const char *fmt, ...)
        __attr_printf__(2, 3);

int btree_check_integrity(btree_t *bt, int dofix,
                          btree_print_fun *fun, FILE *arg);
/* OG: should use enum instead of bool flag for check */
btree_t *btree_open(const char *path, int flags, bool check);
static inline btree_t *btree_creat(const char *path) {
    return btree_open(path, O_CREAT | O_TRUNC | O_RDWR, false);
}
void btree_close(btree_t **tree);

typedef struct { int32_t page; int32_t pos; }  btree_iter_t;

struct bt_key_range {
    int nkeys;
    struct bt_key_range_rec {
        uint64_t key;
        int      dpos;
        int      dlen;
    } *keys;
    union {
        uint8_t  *u8;
        uint32_t *u32;
        void     *data;
    };
};

static inline void btree_key_range_wipe(struct bt_key_range *btkr) {
    p_delete(&btkr->keys);
    p_delete(&btkr->u8);
}

void btree_iter_begin(btree_t *bt, btree_iter_t *iter);
int  btree_iter_next(btree_t *bt, btree_iter_t *iter, uint64_t *key, sb_t *data);

/* OG: Should have both APIs, the default taking a (byte *, len) couple.
 * these should be called btree_fetch_uint64 and btree_push_uint64.
 * Actually, since the implementation seems hardwired for uint64_t
 * keys, the module itself and function/type prefix should be btree64.
 */
int btree_fetch(btree_t *bt, uint64_t key, sb_t *out);
int btree_fetch_range(btree_t *bt, uint64_t kmin, uint64_t kmax,
                      struct bt_key_range *out);
int btree_push(btree_t *bt, uint64_t key, const void *data, int len);

void btree_dump(btree_t *bt, btree_print_fun *fun, FILE *arg);

typedef struct fbtree_t fbtree_t;

fbtree_t *fbtree_open(const char *path);
fbtree_t *fbtree_unlocked_dup(const char *path, btree_t *bt);
int  fbtree_fetch(fbtree_t *, uint64_t key, sb_t *out);
int  fbtree_fetch_range(fbtree_t *, uint64_t kmin, uint64_t kmax,
                        struct bt_key_range *out);
void fbtree_close(fbtree_t **f);

#endif
