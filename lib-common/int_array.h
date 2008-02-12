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

#ifndef IS_LIB_COMMON_INT_ARRAY_H
#define IS_LIB_COMMON_INT_ARRAY_H

#include <stdlib.h>

#include "mem.h"

typedef struct int_array {
    int *tab;
    int len;
    int size;
} int_array;


int_array *int_array_init(int_array *array);
GENERIC_NEW(int_array, int_array);
void int_array_wipe(int_array *array);
GENERIC_DELETE(int_array, int_array);

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

void int_array_resize(int_array *array, int newlen);

void int_array_insert(int_array *array, int pos, int item)
    __attr_nonnull__((1));

static inline void int_array_append(int_array *array, int item) {
    int_array_insert(array, array->len, item);
}

static inline void int_array_push(int_array *array, int item) {
    int_array_insert(array, 0, item);
}

int int_array_take(int_array *array, int pos, int *item)
    __attr_nonnull__((1));
void int_array_remove(int_array *array, int pos)
    __attr_nonnull__((1));

static inline void int_array_swap(int_array *array, int i, int j) {
    int v = array->tab[i];
    array->tab[i] = array->tab[j];
    array->tab[j] = v;
}

static inline void int_array_reset(int_array *array) {
     array->len = 0;
}

#endif /* IS_LIB_COMMON_INT_ARRAY_H */
