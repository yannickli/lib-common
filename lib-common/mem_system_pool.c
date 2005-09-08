#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "err_report.h"
#include "mem_pool.h"

/******************************************************************************/
/* Private API                                                                */
/******************************************************************************/

#define check_enough_mem(mem)                                   \
    do {                                                        \
        if (mem == NULL) {                                      \
            e_fatal(FATAL_NOMEM, E_PREFIX("out of memory"));    \
        }                                                       \
    } while(0)

static void * sp_malloc(size_t size)
{
    void * mem;

    if (size == 0) {
        return NULL;
    }

    mem = malloc(size);
    check_enough_mem(mem);
    return mem;
}

static void * sp_calloc(size_t size)
{
    void * mem;

    if (size == 0) {
        return NULL;
    }

    mem = calloc(size, 1);
    check_enough_mem(mem);
    return mem;
}

static void * sp_free(void * mem)
{
    if (mem != NULL) {
        free(mem);
    }
    return NULL;
}

static void * sp_realloc(void * mem, size_t newsize)
{
    mem = realloc(mem, newsize);
    check_enough_mem(mem);
    return mem;
}

static void * sp_realloc0(void * mem, size_t oldsize, size_t newsize)
{
    mem = realloc(mem, newsize);
    check_enough_mem(mem);
    if (oldsize < newsize) {
        memset((char *) mem + oldsize, 0, newsize - oldsize);
    }
    return mem;
}


static pool_t static_system_pool = {
    "system",
    sp_malloc,
    sp_calloc,
    sp_free,
    sp_realloc,
    sp_realloc0,
};

/******************************************************************************/
/* Public API                                                                 */
/******************************************************************************/

const pool_t * system_pool = &static_system_pool;

