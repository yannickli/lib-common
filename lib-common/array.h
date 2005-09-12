#ifndef IS_ARRAY_H
#define IS_ARRAY_H

#include <stdlib.h>

#include "mem_pool.h"

typedef struct array array_t;
typedef void array_item_dtor_t(void * item);

/******************************************************************************/
/* Memory management                                                          */
/******************************************************************************/

array_t * array_new(const pool_t * pool);
void array_delete(array_t ** array);
void array_delete_all(array_t ** array, array_item_dtor_t * dtor);

/******************************************************************************/
/* Properties                                                                 */
/******************************************************************************/

ssize_t array_len(array_t * array);
void array_append(array_t * array, void * item);

#endif
