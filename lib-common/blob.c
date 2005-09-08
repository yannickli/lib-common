#include "macros.h"
#include "blob.h"
#include "mem_pool.h"

typedef struct {
    /* public interface */
    ssize_t len;
    unsigned char * data;

    /* private interface */
    ssize_t size;  /* allocated size */
    pool_t pool;   /* the pool used for allocation */
} real_blob_t;


/******************************************************************************/
/* Module init                                                                */
/******************************************************************************/

void blob_init(void)
{
}

void blob_deinit(void)
{
}

