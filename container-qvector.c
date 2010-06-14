/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "container.h"

void qvector_reset(qvector_t *vec, int v_size)
{
    vec->size += vec->skip;
    vec->tab  += vec->skip * v_size;
    vec->skip  = 0;
    vec->len   = 0;
}

void qvector_wipe(qvector_t *vec, int v_size)
{
    switch (vec->mem_pool & MEM_POOL_MASK) {
      case MEM_STATIC:
      case MEM_STACK:
        qvector_reset(vec, v_size);
        return;
      default:
        ifree(vec->tab - v_size * vec->skip, vec->mem_pool);
        p_clear(vec, 1);
        return;
    }
}

static void qvector_destroy_skip(qvector_t *vec, int v_size)
{
    memmove(vec->tab - v_size * vec->skip, vec->tab, v_size * vec->len);
    vec->tab -= v_size * vec->skip;
    vec->size += v_size * vec->skip;
    vec->skip  = 0;
}

void __qvector_grow(qvector_t *vec, int v_size, int extra)
{
    int newlen = vec->len + extra;
    int newsz;

    if (unlikely(newlen < 0))
        e_panic("trying to allocate insane amount of memory");

    /* if tab fits and skip is worth it, shift it left */
    if (newlen < vec->skip + vec->size && vec->skip > vec->size / 4) {
        qvector_destroy_skip(vec, v_size);
        return;
    }

    /* most of our pool have expensive reallocs wrt a typical memcpy,
     * and optimize the last realloc so we don't want to alloc and free
     */
    if (vec->mem_pool != MEM_LIBC) {
        qvector_destroy_skip(vec, v_size);
        if (newlen < vec->size)
            return;
    }

    newsz = p_alloc_nr(vec->size + vec->skip);
    if (newsz < newlen)
        newsz = newlen;
    if (vec->mem_pool == MEM_STATIC || vec->skip) {
        uint8_t *s = p_new_raw(uint8_t, newsz * v_size);

        memcpy(s, vec->tab, vec->len * v_size);
        if (vec->mem_pool != MEM_STATIC) {
            /* XXX: If we have vec->skip, then mem_pool == MEM_LIBC */
            libc_free(vec->tab - vec->skip, 0);
        }
        __qvector_init(vec, s, vec->len, newsz, MEM_LIBC);
    } else {
        vec->tab = irealloc(vec->tab, vec->len * v_size, newsz * v_size,
                             vec->mem_pool | MEM_RAW);
        vec->size = newsz;
    }
}

void *__qvector_splice(qvector_t *vec, int v_size, int pos, int len, int dlen)
{
    assert (pos >= 0 && len >= 0 && dlen >= 0);
    assert (pos <= vec->len && pos + len <= vec->len);

    if (len >= dlen) {
        memmove(vec->tab + v_size * (pos + dlen),
                vec->tab + v_size * (pos + len),
                v_size * (vec->len - pos - len));
    } else
    if (len + vec->skip >= dlen) {
        vec->skip -= dlen - len;
        vec->tab -= (dlen - len) * v_size;
        vec->size += dlen - len;
        memmove(vec->tab, vec->tab + v_size * (dlen - len), pos * v_size);
    } else {
        qvector_grow(vec, v_size, dlen - len);
        memmove(vec->tab + v_size * (pos + dlen),
                vec->tab + v_size * (pos + len),
                v_size * (vec->len - pos - len));
    }
    vec->len += dlen - len;
    return vec->tab + pos * v_size;
}
