#include "string_is.h"
#include "array.h"
#include "macros.h"

typedef struct
{
    void ** tab;
    ssize_t len;

    ssize_t size;
    const pool_t * pool;
} real_array_t;

#define ARRAY_INITIAL_SIZE 32
#define REAL(array) ((real_array_t *)(array))

/******************************************************************************/
/* Private inlines                                                            */
/******************************************************************************/

static inline void
array_resize(array_t * array, ssize_t newsize)
{
    real_array_t * a = REAL(array);
    
    if (newsize <= a->size) {
        a->len = newsize;
        return;
    }

    a->size = MEM_ALIGN(newsize);
    a->tab = system_pool->realloc(a->tab, a->size);
}


/******************************************************************************/
/* Memory management                                                          */
/******************************************************************************/

array_t * array_new(const pool_t * pool)
{
    real_array_t * array = p_new(pool, real_array_t, 1);
    array->tab  = p_new0(pool, void*, ARRAY_INITIAL_SIZE);
    array->len  = 0;
    array->size = ARRAY_INITIAL_SIZE;

    return (array_t *)array;
}

void array_delete(array_t ** array)
{
    p_delete(REAL(*array)->pool, *array);
}

void array_delete_all(array_t ** array, array_item_dtor_t * dtor)
{
    ssize_t i;
    for ( i = 0 ; i < (*array)->len ; i++ ) {
        dtor((*array)->tab[i]);
    }
    p_delete(REAL(*array)->pool, *array);
}

/******************************************************************************/
/* Misc                                                                       */
/******************************************************************************/

ssize_t array_len(array_t * array)
{
    return array->len;
}

void array_append(array_t * array, void * item)
{
    ssize_t old_len = array->len;
    array_resize(array, old_len + 1);
    array->tab[old_len] = item;
}

void * array_take_real(array_t * array, ssize_t pos)
{
    void * ptr;
    if (pos > array->len || pos < 0) {
        return NULL;
    }

    ptr = array->tab[pos];
    memmove(array->tab + pos, array->tab + pos + 1, array->len - pos - 1);
    REAL(array)->len --;

    return ptr;
}



