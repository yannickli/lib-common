#ifndef IS_ARRAY_H
#define IS_ARRAY_H

#include <stdlib.h>

#include "mem.h"

typedef struct {
    void ** const tab;
    ssize_t const len;

    /* HERE SO THAT sizeof(array) is ok */
    ssize_t const __size;
} array_t;
typedef void array_item_dtor_f(void * item);

#define array_get(type, array, pos) ((type*)((array)->tab[pos]))

/******************************************************************************/
/* Memory management                                                          */
/******************************************************************************/

#define array_new() array_init(p_new_raw(array_t, 1))
array_t * array_init(array_t * array);
void array_wipe(array_t * array, array_item_dtor_f *dtor);
void array_delete(array_t ** array, array_item_dtor_f *dtor);

/******************************************************************************/
/* Misc                                                                       */
/******************************************************************************/

ssize_t array_len(array_t * array);
void array_append(array_t * array, void * item);
void * array_take_real(array_t * array, ssize_t pos);
#define array_take(type, array, pos) ((type*)(array_take_real(array, pos)))

#endif
