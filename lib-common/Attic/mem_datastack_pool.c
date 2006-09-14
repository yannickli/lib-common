/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <stdlib.h>

#include "macros.h"
#include "err_report.h"
#include "mem_pool.h"
#include "data_stack.h"
#include "string_is.h"

/**************************************************************************/
/* Private API                                                            */
/**************************************************************************/

static void * dsp_malloc(ssize_t size)
{
    return ds_malloc(size);
}

static void * dsp_calloc(ssize_t size)
{
    void * mem = ds_malloc(size);

    memset(mem, 0, size);
    return mem;
}

static void * dsp_free(void * mem)
{
    return mem;
}

static void * dsp_realloc(void * mem, ssize_t newsize)
{
    void * newmem = ds_malloc(newsize);
    ssize_t oldsize = max_safe_size(mem);
    memcpy(newmem, mem, MIN(oldsize, newsize));
    return newmem;
}

static void * dsp_realloc0(void * mem, ssize_t oldsize, ssize_t newsize)
{
    if (oldsize > newsize)
    {
        void * newmem = newmem = ds_malloc(newsize);
        memcpy(newmem, mem, newsize);
        return newmem;
    }
    if (oldsize < newsize) {
        memset((char *) mem + oldsize, 0, newsize - oldsize);
    }
    return mem;
}


static pool_t static_ds_pool = {
    "data-stack",
    dsp_malloc,
    dsp_calloc,
    dsp_free,
    dsp_realloc,
    dsp_realloc0,
};

/**************************************************************************/
/* Public API                                                             */
/**************************************************************************/

const pool_t * const ds_pool = &static_ds_pool;

