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

void mem_register(mem_blk_t *blk)
{
    rb_node_t **n = &blks_g.root;
    rb_node_t *parent = NULL;

    spin_lock(&lock_g);
    while (*n) {
        mem_blk_t *e = rb_entry(parent = *n, mem_blk_t, node);
        int cmp = CMP(blk->start, e->start);

        if (cmp < 0) {
            n = &(*n)->left;
        } else if (cmp > 0) {
            n = &(*n)->right;
        } else {
            assert (false);
        }
    }

    rb_add_node(&blks_g, parent, *n = &blk->node);
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
    rb_node_t *n;

    spin_lock(&lock_g);
    n = blks_g.root;

    while (n) {
        mem_blk_t *e = rb_entry(n, mem_blk_t, node);

        if (addr < e->start) {
            n = n->left;
        } else if ((const char *)addr >= (const char *)e->start + e->size) {
            n = n->right;
        } else {
            spin_unlock(&lock_g);
            return e;
        }
    }

    spin_unlock(&lock_g);

    return NULL;
}

void mem_for_each(mem_pool_t *mp, void (*fn)(mem_blk_t *, void *), void *priv)
{
    spin_lock(&lock_g);
    rb_for_each_safe(it, &blks_g)
        (*fn)(rb_entry(it, mem_blk_t, node), priv);
    spin_unlock(&lock_g);
}

void *__imalloc(size_t size, mem_flags_t flags)
{
    if (size == 0)
        return NULL;
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
        /* FALLTRHOUGH */
      case MEM_LIBC:
        libc_free(mem, flags);
        return;
    }
}

void __irealloc(void **mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    mem_blk_t *blk;

    if (size == 0) {
        ifree(*mem, flags);
        *mem = NULL;
        return;
    }
    if (size > MEM_ALLOC_MAX)
        e_panic("You cannot allocate that amount of memory");

    switch (flags & MEM_POOL_MASK) {
      case MEM_STATIC:
        e_panic("You cannot realloc alloca-ed memory");
      case MEM_STACK:
        stack_realloc(mem, oldsize, size, flags);
        return;
      default:
        blk = mem_blk_find(mem);
        if (blk) {
            blk->pool->realloc(blk->pool, mem, oldsize, size, flags);
            return;
        }
        /* FALLTRHOUGH */
      case MEM_LIBC:
        libc_realloc(mem, oldsize, size, flags);
        return;
    }
}
