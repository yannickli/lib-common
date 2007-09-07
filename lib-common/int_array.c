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

#ifndef MINGCC
#include <alloca.h>
#endif

#include "macros.h"
#include "int_array.h"

#define ARRAY_INITIAL_SIZE 32

/**************************************************************************/
/* Private inlines                                                        */
/**************************************************************************/

static inline void
array_resize(int_array *a, ssize_t newlen)
{
    p_allocgrow(&a->tab, newlen, &a->size);
    p_clear(a->tab + a->len, a->size - a->len);
    a->len = newlen;
}


/**************************************************************************/
/* Memory management                                                      */
/**************************************************************************/

/* OG: should initialize as empty */
int_array *int_array_init(int_array *array)
{
    ssize_t i = 0;

    array->tab  = p_new(int , ARRAY_INITIAL_SIZE);
    array->len  = 0;
    array->size = ARRAY_INITIAL_SIZE;
    while (i < array->size) {
        array->tab[i++] = 0;
    }
    return array;
}

void int_array_wipe(int_array *array)
{
    if (array) {
        p_delete(&array->tab);
    }
}

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

void int_array_resize(int_array *array, ssize_t newlen)
{
    array_resize(array, newlen);
}

int int_array_take(int_array *array, ssize_t pos, int *item)
{
    if (pos >= array->len || pos < 0) {
        return 1;
    }

    *item = array->tab[pos];
    memmove(array->tab + pos, array->tab + pos + 1,
            (array->len - pos - 1) * sizeof(int));
    array->len--;

    return 0;
}

/* insert item at pos `pos',
   pos interpreted as array->len if pos > array->len */
void int_array_insert(int_array *array, ssize_t pos, int item)
{
    ssize_t curlen = array->len;

    array_resize(array, curlen + 1);

    if (pos < curlen) {
        if (pos < 0) {
            pos = 0;
        }
        memmove(array->tab + pos + 1, array->tab + pos,
                (curlen - pos) * sizeof(int));
    } else {
        pos = curlen;
    }

    array->tab[pos] = item;
}
