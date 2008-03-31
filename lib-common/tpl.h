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

/** \defgroup templates Intersec generic templating API.
 *
 * \brief This module defines the #tpl_t type, and its manipulation APIs.
 *
 * \TODO PUT SOME EXPLANATION ON THE PHILOSOPHY OF tpl_t'S HERE.
 *
 *\{*/

/** \file tpl.h
 * \brief Templating module header.
 */

#define TPL_COPY_LIMIT_HARD    32
#define TPL_COPY_LIMIT_SOFT   256
#define TPL_DATA_LIMIT_KEEP  4096

typedef enum tpl_op {
    TPL_OP_DATA = 0x00,
    TPL_OP_BLOB,
    TPL_OP_VAR,

    TPL_OP_BLOCK = 0x10,
    TPL_OP_IFDEF,
    TPL_OP_APPLY_PURE,        /* f(x) only depends upon x */
    TPL_OP_APPLY_DELAYED,     /* assumed pure */
    TPL_OP_APPLY_PURE_ASSOC,  /* also f(a + b) == f(a) + f(b) */
} tpl_op;

struct tpl_data {
    const byte *data;
    int len;
};

struct tpl_t;
typedef int (tpl_apply_f)(struct tpl_t *, blob_t *, struct tpl_t **, int nb);

ARRAY_TYPE(struct tpl_t, tpl);
typedef struct tpl_t {
    flag_t is_const :  1; /* if the subtree has TPL_OP_VARs in it */
    tpl_op op       :  7;
    unsigned refcnt : 24;
    union {
        struct tpl_data data;
        blob_t blob;
        uint32_t varidx; /* 16 bits of env, 16 bits of index */
        struct {
            tpl_apply_f *f;
            tpl_array blocks;
        };
    } u;
} tpl_t;

#define tpl_new()  tpl_new_op(TPL_OP_BLOCK)
tpl_t *tpl_new_op(tpl_op op);
tpl_t *tpl_dup(const tpl_t *);
void tpl_delete(tpl_t **);
ARRAY_FUNCTIONS(tpl_t, tpl, tpl_delete);

/****************************************************************************/
/* Build the AST                                                            */
/****************************************************************************/

blob_t *tpl_get_blob(tpl_t *tpl);

void tpl_add_data(tpl_t *tpl, const byte *data, int len);
void tpl_add_byte(tpl_t *tpl, byte b);
static inline void tpl_add_cstr(tpl_t *tpl, const char *s) {
    tpl_add_data(tpl, (const byte *)s, strlen(s));
}
void tpl_add_fmt(tpl_t *tpl, const char *fmt, ...) __attr_printf__(2, 3);

void tpl_copy_data(tpl_t *tpl, const byte *data, int len);
static inline void tpl_copy_cstr(tpl_t *tpl, const char *s) {
    tpl_copy_data(tpl, (const byte *)s, strlen(s));
}
void tpl_add_var(tpl_t *tpl, uint16_t envid, uint16_t index);
void tpl_add_tpl(tpl_t *out, const tpl_t *tpl);
void tpl_add_tpls(tpl_t *out, tpl_t **tpl, int nb);
tpl_t *tpl_add_ifdef(tpl_t *tpl, uint16_t envid, uint16_t index);
tpl_t *tpl_add_apply(tpl_t *tpl, tpl_op op, tpl_apply_f *f);
void tpl_dump(int dbg, const tpl_t *tpl, const char *s);

/****************************************************************************/
/* Substitution and optimization                                            */
/****************************************************************************/

enum {
    TPL_KEEPVAR    = 1 << 0,
    TPL_LASTSUBST  = 1 << 1,
};

int tpl_get_short_data(tpl_t **tpls, int nb, const byte **data, int *len);

int tpl_fold(blob_t *, tpl_t **, uint16_t envid, tpl_t **, int nb, int flags);
int tpl_fold_str(blob_t *, tpl_t **, uint16_t envid, const char **, int nb, int flags);

int tpl_subst(tpl_t **, uint16_t envid, tpl_t **, int nb, int flags);
int tpl_subst_str(tpl_t **, uint16_t envid, const char **, int nb, int flags);
void tpl_optimize(tpl_t *tpl);

int tpl_to_iov(struct iovec *, int nr, tpl_t *);
int tpl_to_iovec_vector(iovec_vector *iov, tpl_t *tpl);

/****************************************************************************/
/* Checksums                                                                */
/****************************************************************************/

int tpl_compute_len_copy(blob_t *b, tpl_t **args, int nb, int len);

/****************************************************************************/
/* Short formats                                                            */
/****************************************************************************/

int tpl_encode_plmn(tpl_t *out, blob_t *blob, tpl_t **args, int nb);
int tpl_encode_expiration(tpl_t *out, blob_t *blob, tpl_t **args, int nb);

/****************************************************************************/
/* Escapings                                                                */
/****************************************************************************/

int tpl_encode_xml(tpl_t *out, blob_t *blob, tpl_t **args, int nb);
int tpl_encode_ira(tpl_t *out, blob_t *blob, tpl_t **args, int nb);
int tpl_encode_ira_bin(tpl_t *out, blob_t *blob, tpl_t **args, int nb);
int tpl_encode_base64(tpl_t *out, blob_t *blob, tpl_t **args, int nb);
int tpl_encode_qp(tpl_t *out, blob_t *blob, tpl_t **args, int nb);

/**\}*/
#endif
