/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
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

void qvector_reset(qvector_t *vec, size_t v_size)
{
    size_t sz = vec->size;

    if (vec->mem_pool == MEM_LIBC && sz * v_size > (128 << 10)) {
        libc_free(vec->tab, 0);
        p_clear(vec, 1);
    } else {
        __qvector_init(vec, vec->tab, 0, sz, vec->mem_pool);
    }
}

void qvector_wipe(qvector_t *vec, size_t v_size)
{
    switch (vec->mem_pool & MEM_POOL_MASK) {
      case MEM_STATIC:
      case MEM_STACK:
        qvector_reset(vec, v_size);
        return;
      default:
        ifree(vec->tab, vec->mem_pool);
        p_clear(vec, 1);
        return;
    }
}

void __qvector_grow(qvector_t *vec, size_t v_size, int extra)
{
    int newlen = vec->len + extra;
    int newsz;

    if (unlikely(newlen < 0))
        e_panic("trying to allocate insane amount of memory");

    /* if tab fits, return */
    if (newlen < vec->size) {
        return;
    }

    newsz = p_alloc_nr(vec->size);
    if (newsz < newlen)
        newsz = newlen;
    if (vec->mem_pool == MEM_STATIC) {
        uint8_t *s = p_new_raw(uint8_t, newsz * v_size);

        memcpy(s, vec->tab, vec->len * v_size);
        __qvector_init(vec, s, vec->len, newsz, MEM_LIBC);
    } else {
        vec->tab = irealloc(vec->tab, vec->len * v_size, newsz * v_size,
                             vec->mem_pool | MEM_RAW);
        vec->size = newsz;
    }
}

void __qvector_optimize(qvector_t *vec, size_t v_size, size_t size)
{
    char *buf;

    if (size == 0) {
        qvector_reset(vec, v_size);
        return;
    }
    if (vec->mem_pool != MEM_LIBC)
        return;
    buf = p_new_raw(char, size * v_size);
    memcpy(buf, vec->tab, vec->len * v_size);
    libc_free(vec->tab, 0);
    __qvector_init(vec, buf, vec->len, size, MEM_LIBC);
}

void *__qvector_splice(qvector_t *vec, size_t v_size, int pos, int len, int dlen)
{
    assert (pos >= 0 && len >= 0 && dlen >= 0);
    assert (pos <= vec->len && pos + len <= vec->len);

    if (len >= dlen) {
        memmove(vec->tab + v_size * (pos + dlen),
                vec->tab + v_size * (pos + len),
                v_size * (vec->len - pos - len));
        qvector_grow(vec, v_size, 0);
    } else {
        qvector_grow(vec, v_size, dlen - len);
        memmove(vec->tab + v_size * (pos + dlen),
                vec->tab + v_size * (pos + len),
                v_size * (vec->len - pos - len));
    }
    vec->len += dlen - len;
    return vec->tab + pos * v_size;
}
