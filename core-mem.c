/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "core-mem-valgrind.h"

void *libc_malloc(size_t size, mem_flags_t flags)
{
    void *res;

    if (flags & MEM_RAW) {
        res = malloc(size);
    } else {
        res = calloc(1, size);
    }
    if (unlikely(res == NULL)) {
        if (flags & MEM_ERRORS_OK)
            return NULL;
        e_panic("out of memory");
    }
    return res;
}

void *libc_realloc(void *mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    byte *res = realloc(mem, size);

    if (unlikely(res == NULL)) {
        if (!size || (flags & MEM_ERRORS_OK))
            return NULL;
        e_panic("out of memory");
    }
    if (!(flags & MEM_RAW) && oldsize < size)
        memset(res + oldsize, 0, size - oldsize);
    return res;
}

void *__imalloc(size_t size, mem_flags_t flags)
{
    if (size > MEM_ALLOC_MAX)
        e_panic("You cannot allocate that amount of memory");
    switch (flags & MEM_POOL_MASK) {
      case MEM_STATIC:
      default:
        e_panic("You cannot allocate from pool %d with imalloc",
                flags & MEM_POOL_MASK);
      case MEM_LIBC:
        return libc_malloc(size, flags);
      case MEM_STACK:
        return stack_malloc(size, flags);
    }
}

void __ifree(void *mem, mem_flags_t flags)
{
    switch (flags & MEM_POOL_MASK) {
      case MEM_STATIC:
      case MEM_STACK:
        return;
      case MEM_LIBC:
        libc_free(mem, flags);
        return;
      default:
        e_panic("pool memory cannot be deallocated with ifree");
    }
}

void *__irealloc(void *mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    if (size == 0) {
        ifree(mem, flags);
        return NULL;
    }
    if (size > MEM_ALLOC_MAX)
        e_panic("You cannot allocate that amount of memory");

    switch (flags & MEM_POOL_MASK) {
      case MEM_STATIC:
        e_panic("You cannot realloc alloca-ed memory");
      case MEM_STACK:
        return stack_realloc(mem, oldsize, size, flags);
      case MEM_LIBC:
        return libc_realloc(mem, oldsize, size, flags);
      default:
        e_panic("pool memory cannot be reallocated with ifree");
    }
}

extern const char libcommon_id[];
const char *__libcomon_version(void);
const char *__libcomon_version(void)
{
    return libcommon_id;
}
