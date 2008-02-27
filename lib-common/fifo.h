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

#ifndef IS_LIB_COMMON_FIFO_H
#define IS_LIB_COMMON_FIFO_H

#include "mem.h"

typedef struct fifo {
    void **elems;
    int len;
    int first;
    int size;
} fifo;
typedef void fifo_item_dtor_f(void *item);

GENERIC_INIT(fifo, fifo);
GENERIC_NEW(fifo, fifo);
void fifo_wipe(fifo *f, fifo_item_dtor_f *dtor);
void fifo_delete(fifo **f, fifo_item_dtor_f *dtor);

void *fifo_get(fifo *f)             __attr_nonnull__((1));
void fifo_unget(fifo *f, void *ptr) __attr_nonnull__((1));
void fifo_put(fifo *f, void *ptr)   __attr_nonnull__((1));
void *fifo_seti(fifo *f, int i, void *ptr) __attr_nonnull__((1));
void *fifo_geti(fifo *f, int i) __attr_nonnull__((1));

/**************************************************************************/
/* Typed Fifos                                                            */
/**************************************************************************/

#define FIFO_TYPE(el_typ, prefix)                                            \
    typedef struct prefix##_fifo {                                           \
        el_typ ** const elems;                                               \
        int len;                                                             \
        int first;                                                           \
        int size;                                                            \
    } prefix##_fifo

#define FIFO_FUNCTIONS(el_typ, prefix, dtor)                                 \
                                                                             \
    /* legacy functions */                                                   \
    static inline prefix##_fifo *prefix##_fifo_new(void)                     \
    {                                                                        \
        return (prefix##_fifo *)fifo_new();                                  \
    }                                                                        \
    static inline prefix##_fifo *                                            \
    prefix##_fifo_init(prefix##_fifo *f)                                     \
    {                                                                        \
        return (prefix##_fifo *)fifo_init((fifo *)f);                        \
    }                                                                        \
    static inline void                                                       \
    prefix##_fifo_wipe(prefix##_fifo *f)                                     \
    {                                                                        \
        fifo_wipe((fifo*)f, (fifo_item_dtor_f *)dtor);                       \
    }                                                                        \
    static inline void                                                       \
    prefix##_fifo_delete(prefix##_fifo **f, bool do_elts)                    \
    {                                                                        \
        fifo_delete((fifo **)f, (fifo_item_dtor_f *)dtor);                   \
    }                                                                        \
                                                                             \
    /* module functions */                                                   \
    static inline void                                                       \
    prefix##_fifo_put(prefix##_fifo *f, el_typ *item)                        \
    {                                                                        \
        fifo_put((fifo *)f, (void*)item);                                    \
    }                                                                        \
    static inline el_typ *                                                   \
    prefix##_fifo_get(prefix##_fifo *f)                                      \
    {                                                                        \
        return (el_typ *)fifo_get((fifo *)f);                                \
    }                                                                        \
    static inline void                                                       \
    prefix##_fifo_unget(prefix##_fifo *f, el_typ *item)                      \
    {                                                                        \
        fifo_unget((fifo *)f, (void*)item);                                  \
    }                                                                        \
    static inline el_typ *                                                   \
    prefix##_fifo_seti(prefix##_fifo *f, int i, el_typ *item)                \
    {                                                                        \
        return fifo_seti((fifo *)f, i, (void*)item);                         \
    }                                                                        \
    static inline el_typ *                                                   \
    prefix##_fifo_geti(prefix##_fifo *f, int i)                              \
    {                                                                        \
        return fifo_geti((fifo *)f, i);                                      \
    }

#define DO_FIFO(el_typ, prefix, dtor) \
    FIFO_TYPE(el_typ, prefix); FIFO_FUNCTIONS(el_typ, prefix, dtor)

#ifdef CHECK
#include <check.h>

Suite *check_fifo_suite(void);

#endif
#endif /* IS_LIB_COMMON_FIFO_H */
