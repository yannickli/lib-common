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

#include "macros.h"
#include "string_is.h"
#include "array.h"

#define ARRAY_INITIAL_SIZE 32

/**************************************************************************/
/* Private inlines                                                        */
/**************************************************************************/

static inline void
array_resize(generic_array *a, ssize_t newlen)
{
    ssize_t curlen = a->len;

    /* Reallocate array if needed */
    if (newlen > a->size) {
        /* OG: Should use p_realloc */
        /* FIXME: should increase array size more at a time:
         * expand by half the current size?
         */
        a->size = MEM_ALIGN(newlen);
        a->tab = (void **)mem_realloc(a->tab, a->size * sizeof(void*));
    }
    /* Initialize new elements to NULL */
    while (curlen < newlen) {
        a->tab[curlen++] = NULL;
    }
    a->len = newlen;
}


/**************************************************************************/
/* Memory management                                                      */
/**************************************************************************/

generic_array *generic_array_init(generic_array *array)
{
    array->tab  = p_new(void *, ARRAY_INITIAL_SIZE);
    array->len  = 0;
    array->size = ARRAY_INITIAL_SIZE;

    return array;
}

void generic_array_wipe(generic_array *array, array_item_dtor_f *dtor)
{
    if (array) {
        if (dtor) {
            ssize_t i;

            for (i = 0; i < array->len; i++) {
                (*dtor)(&array->tab[i]);
            }
        }
        p_delete(&array->tab);
    }
}

void generic_array_delete(generic_array **array, array_item_dtor_f *dtor)
{
    if (*array) {
        generic_array_wipe(*array, dtor);
        p_delete(&*array);
    }
}

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

void *generic_array_take(generic_array *array, ssize_t pos)
{
    void *ptr;

    if (pos >= array->len || pos < 0) {
        return NULL;
    }

    ptr = array->tab[pos];
    memmove(array->tab + pos, array->tab + pos + 1,
            (array->len - pos - 1) * sizeof(void*));
    array->len--;

    return ptr;
}

/* insert item at pos `pos',
   pos interpreted as array->len if pos > array->len */
void generic_array_insert(generic_array *array, ssize_t pos, void *item)
{
    ssize_t curlen = array->len;

    array_resize(array, curlen + 1);

    if (pos < curlen) {
        if (pos < 0) {
            pos = 0;
        }
        memmove(array->tab + pos + 1, array->tab + pos,
                (curlen - pos) * sizeof(void *));
    } else {
        pos = curlen;
    }

    array->tab[pos] = item;
}
