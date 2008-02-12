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

#define TPL_COPY_LIMIT_HARD    32
#define TPL_COPY_LIMIT_SOFT   256
#define TPL_DATA_LIMIT_KEEP  4096

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
    const byte *data;
    int len;
};

struct tpl_t;
typedef int (tpl_apply_f)(struct tpl_t *, struct tpl_t *);

ARRAY_TYPE(struct tpl_t, tpl);
typedef struct tpl_t {
    flag_t no_subst :  1; /* if the subtree has TPL_OP_VARs in it */
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

tpl_t *tpl_new(void);
tpl_t *tpl_dup(const tpl_t *);
void tpl_delete(tpl_t **);
ARRAY_FUNCTIONS(tpl_t, tpl, tpl_delete);

/****************************************************************************/
/* Build the AST                                                            */
/****************************************************************************/

void tpl_add_data(tpl_t *tpl, const byte *data, int len);
static inline void tpl_add_cstr(tpl_t *tpl, const char *s) {
    tpl_add_data(tpl, (const byte *)s, strlen(s));
}
static inline void tpl_add_byte(tpl_t *tpl, byte b)
{
      return tpl_add_data(tpl, &b, 1);
}

void tpl_copy_data(tpl_t *tpl, const byte *data, int len);
static inline void tpl_copy_cstr(tpl_t *tpl, const char *s) {
    tpl_copy_data(tpl, (const byte *)s, strlen(s));
}
void tpl_add_var(tpl_t *tpl, uint16_t envid, uint16_t index);
void tpl_add_tpl(tpl_t *out, const tpl_t *tpl);
tpl_t *tpl_add_ifdef(tpl_t *tpl, uint16_t envid, uint16_t index);
tpl_t *tpl_add_apply(tpl_t *tpl, tpl_op op, tpl_apply_f *f);
void tpl_dump(int dbg, const tpl_t *tpl, const char *s);

blob_t *tpl_get_blob(tpl_t *tpl);

/****************************************************************************/
/* Substitution and optimization                                            */
/****************************************************************************/

int tpl_get_short_data(tpl_t *tpl, const byte **data, int *len);

tpl_t *tpl_subst(const tpl_t *, uint16_t envid, tpl_t **, int nb, int keepvars);
void tpl_optimize(tpl_t *tpl);

#endif
