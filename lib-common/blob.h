#ifndef IS_BLOB_H
#define IS_BLOB_H

#include <unistd.h>

#include "mem_pool.h"

typedef struct {
    const ssize_t len;
    const unsigned char * const data;
} blob_t;


/******************************************************************************/
/* Module init                                                                */
/******************************************************************************/

void blob_init(void);
void blob_deinit(void);

#endif
