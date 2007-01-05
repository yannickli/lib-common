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

#ifndef IS_LIB_COMMON_FIFO_H
#define IS_LIB_COMMON_FIFO_H

#include <stdlib.h>

#include "mem.h"

typedef struct fifo {
    void ** elems;
    ssize_t nb_elems;
    ssize_t first;

    ssize_t size;
} fifo;
typedef void fifo_item_dtor_f(void *item);

fifo *fifo_init(fifo *f);
fifo *fifo_init_nb(fifo *f, ssize_t size);
GENERIC_NEW(fifo, fifo);
void fifo_wipe(fifo *f, fifo_item_dtor_f *dtor);
void fifo_delete(fifo **f, fifo_item_dtor_f *dtor);

void *fifo_get(fifo *f)            __attr_nonnull__((1));
void fifo_put(fifo *f, void *ptr)  __attr_nonnull__((1));

/**************************************************************************/
/* Typed Fifos                                                            */
/**************************************************************************/

#define FIFO_TYPE(el_typ, prefix)                                            \
    typedef struct prefix##_fifo {                                           \
        el_typ ** const elems;                                               \
        ssize_t nb_elems;                                                    \
        ssize_t first;                                                       \
        ssize_t size;                                                        \
    } prefix##_fifo

#define FIFO_FUNCTIONS(el_typ, prefix)                                       \
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
    static inline prefix##_fifo *                                            \
    prefix##_fifo_init_nb(prefix##_fifo *f, ssize_t size)                    \
    {                                                                        \
        return (prefix##_fifo *)fifo_init_nb((fifo *)f, size);               \
    }                                                                        \
    static inline void                                                       \
    prefix##_fifo_wipe(prefix##_fifo *f, bool do_elts)                       \
    {                                                                        \
        fifo_wipe((fifo*)f,                                                  \
                do_elts ? (fifo_item_dtor_f *)prefix##_delete : NULL);       \
    }                                                                        \
    static inline void                                                       \
    prefix##_fifo_delete(prefix##_fifo **f, bool do_elts)                    \
    {                                                                        \
        fifo_delete((fifo **)f,                                              \
                do_elts ? (fifo_item_dtor_f *)prefix##_delete : NULL);       \
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
    }

#ifdef CHECK
#include <check.h>

Suite *check_fifo_suite(void);

#endif
#endif /* IS_LIB_COMMON_FIFO_H */
