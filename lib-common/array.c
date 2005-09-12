#include "array.h"
#include "macros.h"

struct array
{
    void ** tab;
    ssize_t len;
    ssize_t size;

    const pool_t * pool;
};

#define ARRAY_INITIAL_SIZE 32

/******************************************************************************/
/* Private inlines                                                            */
/******************************************************************************/

static inline void
array_resize(array_t * array, ssize_t newsize)
{
    if (newsize <= array->size) {
        array->len = newsize;
        return;
    }

    array->size = MEM_ALIGN(newsize);
    array->tab = system_pool->realloc(array->tab, array->size);
}


/******************************************************************************/
/* Memory management                                                          */
/******************************************************************************/

array_t * array_new(const pool_t * pool)
{
    array_t * array = p_new(pool, array_t, 1);
    array->tab  = p_new0(pool, void*, ARRAY_INITIAL_SIZE);
    array->len  = 0;
    array->size = ARRAY_INITIAL_SIZE;

    return array;
}

void array_delete(array_t ** array)
{
    p_delete((*array)->pool, *array);
}

void array_delete_all(array_t ** array, array_item_dtor_t * dtor)
{
    ssize_t i;
    for ( i = 0 ; i < (*array)->len ; i++ ) {
        dtor((*array)->tab[i]);
    }
    p_delete((*array)->pool, *array);
}

/******************************************************************************/
/* Properties                                                                 */
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
