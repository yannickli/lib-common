/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2009 INTERSEC SAS                                  */
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

static rb_t blks_g;
static spinlock_t lock_g;

static RB_INSERT_I(mem_blk_t, mem_blk, node, start);
static RB_SEARCH_I(mem_blk_t, mem_blk, node, start);

void mem_register(mem_blk_t *blk)
{
    rb_node_t **n;

    spin_lock(&lock_g);
    n = mem_blk_insert(&blks_g, blk);
    assert (!n);
    spin_unlock(&lock_g);
    VALGRIND_REG_BLK(blk);
}

void mem_unregister(mem_blk_t *blk)
{
    spin_lock(&lock_g);
    rb_del_node(&blks_g, &blk->node);
    spin_unlock(&lock_g);
    VALGRIND_UNREG_BLK(blk);
}

mem_blk_t *mem_blk_find(const void *addr)
{
    mem_blk_t *res;

    spin_lock(&lock_g);
    res = mem_blk_search(blks_g, addr);
    spin_unlock(&lock_g);
    return res;
}

void mem_for_each(mem_pool_t *mp, void (*fn)(mem_blk_t *, void *), void *priv)
{
    spin_lock(&lock_g);
    rb_for_each_safe(it, &blks_g)
        (*fn)(rb_entry(it, mem_blk_t, node), priv);
    spin_unlock(&lock_g);
}

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
    mem_blk_t *blk;

    switch (flags & MEM_POOL_MASK) {
      case MEM_STATIC:
      case MEM_STACK:
        return;
      default:
        blk = mem_blk_find(mem);
        if (blk) {
            blk->pool->free(blk->pool, mem, flags);
            return;
        }
        /* FALL THRU */
      case MEM_LIBC:
        libc_free(mem, flags);
        return;
    }
}

void *__irealloc(void *mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    mem_blk_t *blk;

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
      default:
        blk = mem_blk_find(mem);
        if (blk)
            return blk->pool->realloc(blk->pool, mem, oldsize, size, flags);
        /* FALL THRU */
      case MEM_LIBC:
        return libc_realloc(mem, oldsize, size, flags);
    }
}

#include "core-version.c"
