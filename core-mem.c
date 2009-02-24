/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
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

void mem_register(mem_blk_t *blk)
{
    rb_node_t **n = &blks_g.root;

    while (*n) {
        mem_blk_t *e = rb_entry(*n, mem_blk_t, node);
        int cmp = CMP(blk->start, e->start);

        if (cmp < 0) {
            n = &(*n)->left;
        } else if (cmp > 0) {
            n = &(*n)->right;
        } else {
            assert (false);
        }
    }

    rb_add_node(&blks_g, n, &blk->node);
    VALGRIND_REG_BLK(blk);
}

void mem_unregister(mem_blk_t *blk)
{
    rb_del_node(&blks_g, &blk->node);
    VALGRIND_UNREG_BLK(blk);
}

mem_blk_t *mem_blk_find(const void *addr)
{
    rb_node_t *n = blks_g.root;

    while (n) {
        mem_blk_t *e = rb_entry(n, mem_blk_t, node);

        if (addr < e->start) {
            n = n->left;
        } else if ((const char *)addr >= (const char *)e->start + e->size) {
            n = n->right;
        } else {
            return e;
        }
    }

    return NULL;
}

void mem_for_each(mem_pool_t *mp, void (*fn)(mem_blk_t *, void *), void *priv)
{
    rb_for_each_safe(it, &blks_g)
        (*fn)(rb_entry(it, mem_blk_t, node), priv);
}

void *__imalloc(size_t size, mem_flags_t flags)
{
    if (size == 0)
        return NULL;
    if (size > MEM_ALLOC_MAX)
        e_panic("You cannot allocate that amount of memory");
    if (flags & MEM_RAW) {
        return malloc(size);
    } else {
        return calloc(1, size);
    }
}

void __ifree(void *mem, mem_flags_t flags)
{
    mem_blk_t *blk;

    if (flags & MEM_LIBC) {
      libc:
        free(mem);
        return;
    }
    blk = mem_blk_find(mem);
    if (!blk)
        goto libc;
    blk->pool->free(blk->pool, mem, flags);
}

void __irealloc(void **mem, size_t oldsize, size_t size, mem_flags_t flags)
{
    mem_blk_t *blk;
    char *res;

    if (size == 0) {
        ifree(*mem, flags);
        *mem = NULL;
        return;
    }
    if (size > MEM_ALLOC_MAX)
        e_panic("You cannot allocate that amount of memory");
    if (flags & MEM_LIBC) {
      libc:
        res = realloc(*mem, size);
        if (unlikely(res == NULL))
            e_panic("out of memory");
        if (!(flags & MEM_RAW) && oldsize < size)
            memset(res + oldsize, 0, size - oldsize);
        *mem = res;
        return;
    }

    blk = mem_blk_find(mem);
    if (!blk)
        goto libc;
    blk->pool->realloc(blk->pool, mem, oldsize, size, flags);
}
