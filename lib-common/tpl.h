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

#ifndef IS_LIB_COMMON_TPL_H
#define IS_LIB_COMMON_TPL_H

#include "array.h"
#include "blob.h"
#include "ioveclist.h"

typedef enum tpl_op {
    TPL_OP_DATA = 0x00,
    TPL_OP_BLOB,
    TPL_OP_VAR,

    TPL_OP_BLOCK = 0x10,
    TPL_OP_IFDEF,
    TPL_OP_APPLY,
    TPL_OP_APPLY_PURE,        /* f(x) only depends upon x */
    TPL_OP_APPLY_DELAYED,     /* assumed pure */
    TPL_OP_APPLY_PURE_ASSOC,  /* also f(a + b) == f(a) + f(b) */
} tpl_op;

struct tpl_data {
    struct iovec *iov;
    int n, sz;
};

static inline void tpl_data_wipe(struct tpl_data *td) {
    p_delete(&td->iov);
}

typedef int (tpl_apply_f)(blob_t *, struct tpl_data *);

ARRAY_TYPE(union tpl_t, tpl);
typedef union tpl_t {
    int refcnt;
    tpl_op op : 8;
    union {
        struct tpl_data data;
        blob_t blob;
        uint32_t varidx; /* 16 bits of env, 16 bits of index */
        tpl_apply_f *f;
    } u;
    tpl_array blocks;
} tpl_t;

tpl_t *tpl_new(void);
tpl_t *tpl_dup(const tpl_t *);
void tpl_delete(tpl_t **);
ARRAY_FUNCTIONS(tpl_t, tpl, tpl_delete);

/****************************************************************************/
/* Build the AST                                                            */
/****************************************************************************/

void tpl_add_data(tpl_t *tpl, const byte *data, int len);
void tpl_copy_data(tpl_t *tpl, const byte *data, int len);
void tpl_add_var(tpl_t *tpl, uint16_t array, uint16_t index);
void tpl_add_tpl(tpl_t *out, const tpl_t *tpl);
tpl_t *tpl_add_ifdef(tpl_t *tpl, uint16_t array, uint16_t index);
tpl_t *tpl_add_apply(tpl_t *tpl, tpl_op op, tpl_apply_f *f);

/****************************************************************************/
/* Substitution and optimization                                            */
/****************************************************************************/

tpl_t *tpl_subst(const tpl_t *, uint16_t envid, const tpl_t **, int nb);

#endif
