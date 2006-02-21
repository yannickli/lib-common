#ifndef IS_MEM_H
#define IS_MEM_H

#include <unistd.h>
#include <stdlib.h>

#include "macros.h"
#include "err_report.h"
#include "string_is.h"


#define check_enough_mem(mem)                                   \
    do {                                                        \
        if ((mem) == NULL) {                                    \
            e_fatal(FATAL_NOMEM, E_PREFIX("out of memory"));    \
        }                                                       \
    } while(0)

static inline
void * mem_alloc(ssize_t size)
{
    void * mem;

    if (size == 0) {
        return NULL;
    }

    mem = malloc(size);
    check_enough_mem(mem);
    return mem;
}

static inline
void * mem_alloc0(ssize_t size)
{
    void * mem;

    if (size == 0) {
        return NULL;
    }

    mem = calloc(size, 1);
    check_enough_mem(mem);
    return mem;
}

static inline
void * mem_free(void * mem)
{
    if (mem != NULL) {
        free(mem);
    }
    return NULL;
}

static inline
void * mem_realloc(void * mem, ssize_t newsize)
{
    mem = realloc(mem, newsize);
    check_enough_mem(mem);
    return mem;
}

static inline
void * mem_realloc0(void * mem, ssize_t oldsize, ssize_t newsize)
{
    mem = realloc(mem, newsize);
    check_enough_mem(mem);
    if (oldsize < newsize) {
        memset((char *) mem + oldsize, 0, newsize - oldsize);
    }
    return mem;
}

#define p_blank(type, p, count) ((type *)memset(p, 0, sizeof(type)))
#define p_new_raw(type, count)  ((type*)mem_alloc(sizeof(type)*(count)))
#define p_new(type, count)      ((type*)mem_alloc0(sizeof(type)*(count)))
/* FIXME : p_delete multi-evaluates mem_p, and thus has nasty sides effects when
 *         argument has a -- or a ++.
 *         cpp SHOULD have a warning for such macros !!!
 */
#define p_delete(mem_p)         (mem_free(*(mem_p)), *(mem_p) = NULL)

/* OG: should find a better name */
#define p_renew(type, mem, oldcount, newcount) \
    ((type *)mem_realloc0((mem), (oldcount) * sizeof(type), \
                          (newcount) * sizeof(type)))

#define GENERIC_DELETE(wiper, var)  \
    do {                            \
        if (var) {                  \
            (wiper)(*(var));        \
            p_delete(var);          \
        }                           \
    } while(0)

#endif
