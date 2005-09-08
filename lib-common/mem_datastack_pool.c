#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "err_report.h"
#include "mem_pool.h"
#include "data_stack.h"

/******************************************************************************/
/* Private API                                                                */
/******************************************************************************/

static void * dsp_malloc(ssize_t size)
{
    return ds_get(size);
}

static void * dsp_calloc(ssize_t size)
{
    void * mem = ds_get(size);

    memset(mem, 0, size);
    return mem;
}

static void * dsp_free(void * mem)
{
    return mem;
}

static void * dsp_realloc(void * mem, ssize_t newsize)
{
    void * newmem;
    if ((newmem = ds_try_reget(newmem, newsize)) == NULL) {
        ssize_t oldsize = max_safe_size(mem);
        newmem = ds_get(newsize);
        memcpy(newmem, mem, MIN(oldsize, newsize));
    }
    return newmem;
}

static void * dsp_realloc0(void * mem, ssize_t oldsize, ssize_t newsize)
{
    void * newmem;
    if ((newmem = ds_try_reget(newmem, newsize)) == NULL) {
        newmem = ds_get(newsize);
        memcpy(newmem, mem, MIN(oldsize, newsize));
    }
    if (oldsize < newsize) {
        memset((char *) mem + oldsize, 0, newsize - oldsize);
    }
    return newmem;
}


static pool_t static_ds_pool = {
    "data-stack",
    dsp_malloc,
    dsp_calloc,
    dsp_free,
    dsp_realloc,
    dsp_realloc0,
};

/******************************************************************************/
/* Public API                                                                 */
/******************************************************************************/

const pool_t * ds_pool = &static_ds_pool;

