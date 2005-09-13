#ifndef IS_ARRAY_H
#define IS_ARRAY_H

#include <stdlib.h>

#include "mem.h"

typedef struct {
    void ** const tab;
    const ssize_t len;

    /* HERE SO THAT sizeof(array) is ok */
    const ssize_t         dont_use1;
} array_t;
typedef void array_item_dtor_t(void * item);

#define array_get(type, array, pos) ((type*)((array)->tab[pos]))

/******************************************************************************/
/* Memory management                                                          */
/******************************************************************************/

array_t * array_new(void);
void array_delete(array_t ** array);
void array_delete_all(array_t ** array, array_item_dtor_t * dtor);

/******************************************************************************/
/* Misc                                                                       */
/******************************************************************************/

ssize_t array_len(array_t * array);
void array_append(array_t * array, void * item);
void * array_take_real(array_t * array, ssize_t pos);
#define array_take(type, array, pos) ((type*)(array_take_real(array, pos)))

#endif
