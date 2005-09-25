#include "string_is.h"
#include "array.h"
#include "macros.h"

typedef struct
{
    void ** tab;
    ssize_t len;

    ssize_t size;
} real_array;

#define ARRAY_INITIAL_SIZE 32
static inline real_array *array_real(_array *array)
{
    return (real_array *)array;
}

/******************************************************************************/
/* Private inlines                                                            */
/******************************************************************************/

static inline void
array_resize(_array *array, ssize_t newsize)
{
    real_array *a = array_real(array);
    
    if (newsize <= a->size) {
        a->len = newsize;
        return;
    }

    a->size = MEM_ALIGN(newsize);
    a->tab  = mem_realloc(a->tab, a->size*sizeof(void*));
}


/******************************************************************************/
/* Memory management                                                          */
/******************************************************************************/

_array *array_init(_array *array)
{
    real_array *rarray = array_real(array);
    rarray->tab  = p_new(void*, ARRAY_INITIAL_SIZE);
    rarray->len  = 0;
    rarray->size = ARRAY_INITIAL_SIZE;

    return (_array *)array;
}

void array_wipe(_array *array, array_item_dtor_f *dtor)
{
    if (array) {
        if (dtor) {
            ssize_t i;

            for (i = 0 ; i < array->len ; i++ ) {
                (*dtor)(array->tab[i]);
            }
        }
        p_delete(&(array_real(array)->tab));
    }
}

void array_delete(_array **array, array_item_dtor_f *dtor)
{
    if (*array) {
        array_wipe(*array, dtor);
        p_delete(array);
    }
}

/******************************************************************************/
/* Misc                                                                       */
/******************************************************************************/

void array_append(_array *array, void *item)
{
    ssize_t old_len = array->len;
    array_resize(array, old_len + 1);
    array->tab[old_len] = item;
}

void *arrayake(_array *array, ssize_t pos)
{
    void *ptr;
    if (pos > array->len || pos < 0) {
        return NULL;
    }

    ptr = array->tab[pos];
    memmove(array->tab + pos, array->tab + pos + 1, array->len - pos - 1);
    array_real(array)->len --;

    return ptr;
}


