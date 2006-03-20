#include "string_is.h"
#include "array.h"
#include "macros.h"

typedef struct {
    void ** tab;
    ssize_t len;

    ssize_t size;
} real_array;

#define ARRAY_INITIAL_SIZE 32
static inline real_array *array_real(_array *array) {
    return (real_array *)array;
}

/******************************************************************************/
/* Private inlines                                                            */
/******************************************************************************/

static inline void
array_resize(_array *array, ssize_t newlen)
{
    real_array *a = array_real(array);
    ssize_t curlen = a->len;
    
    /* reallocate array if needed */
    if (newlen > a->size) {
        /* OG: Should use p_realloc */
        a->size = MEM_ALIGN(newlen);
        a->tab = (void **)mem_realloc(a->tab, a->size * sizeof(void*));
    }
    /* initialize new elements to NULL */
    while (curlen < newlen)
        a->tab[curlen++] = NULL;
    a->len = newlen;
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

            for (i = 0 ; i < array->len ; i++) {
                (*dtor)(&array->tab[i]);
            }
        }
        p_delete(&(array_real(array)->tab));
    }
}

void array_delete(_array **array, array_item_dtor_f *dtor)
{
    if (*array) {
        array_wipe(*array, dtor);
        p_delete(&*array);
    }
}

/******************************************************************************/
/* Misc                                                                       */
/******************************************************************************/

void *array_take(_array *array, ssize_t pos)
{
    void *ptr;

    if (pos >= array->len || pos < 0) {
        return NULL;
    }

    ptr = array->tab[pos];
    memmove(array->tab + pos, array->tab + pos + 1,
            (array->len - pos - 1) * sizeof(void*));
    array_real(array)->len --;

    return ptr;
}

/* insert item at pos `pos',
   pos interpreted as array->len if pos > array->len */
void array_insert(_array *array, ssize_t pos, void *item)
{
    ssize_t curlen = array->len;

    array_resize(array, curlen + 1);

    if (pos < curlen) {
        memmove(array->tab + pos + 1, array->tab + pos,
                (curlen - pos) * sizeof(void *));
    } else {
        pos = curlen;
    }

    array->tab[pos] = item;
}
