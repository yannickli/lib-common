#include "string_is.h"
#include "array.h"
#include "macros.h"

typedef struct
{
    void ** tab;
    ssize_t len;

    ssize_t size;
} real_array_t;

#define ARRAY_INITIAL_SIZE 32
static inline real_array_t *array_real(array_t *array)
{
    return (real_array_t *)array;
}

/******************************************************************************/
/* Private inlines                                                            */
/******************************************************************************/

static inline void
array_resize(array_t * array, ssize_t newsize)
{
    real_array_t * a = array_real(array);
    
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

array_t * array_init(array_t * array)
{
    real_array_t * rarray = array_real(array);
    rarray->tab  = p_new(void*, ARRAY_INITIAL_SIZE);
    rarray->len  = 0;
    rarray->size = ARRAY_INITIAL_SIZE;

    return (array_t *)array;
}

void array_wipe(array_t *array, array_item_dtor_f *dtor)
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

void array_delete(array_t **array, array_item_dtor_f *dtor)
{
    if (*array) {
        array_wipe(*array, dtor);
        p_delete(array);
    }
}

/******************************************************************************/
/* Misc                                                                       */
/******************************************************************************/

void array_append(array_t * array, void * item)
{
    ssize_t old_len = array->len;
    array_resize(array, old_len + 1);
    array->tab[old_len] = item;
}

void * array_take(array_t * array, ssize_t pos)
{
    void * ptr;
    if (pos > array->len || pos < 0) {
        return NULL;
    }

    ptr = array->tab[pos];
    memmove(array->tab + pos, array->tab + pos + 1, array->len - pos - 1);
    array_real(array)->len --;

    return ptr;
}


