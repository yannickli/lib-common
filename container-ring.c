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

void generic_ring_ensure(generic_ring *r, int newlen, int el_siz)
{
    int cursize = r->size;

    if (unlikely(newlen < 0 || newlen * el_siz < 0))
        e_panic("trying to allocate insane amount of RAM");

    if (newlen <= r->size)
        return;

    r->size = p_alloc_nr(r->size);
    if (r->size < newlen)
        r->size = newlen;
    r->tab = irealloc(r->tab, r->len * el_siz, r->size * el_siz, MEM_RAW | MEM_LIBC);

    /* if elements are split in two parts. Move the shortest one */
    if (r->first + r->len > cursize) {
        char *base = r->tab;
        int right_len = cursize - r->first;
        int left_len  = r->len - right_len;

        if (left_len > right_len || left_len > r->size - cursize) {
            memmove(base + el_siz * (r->size - right_len),
                    base + el_siz * r->first, el_siz * right_len);
            r->first = r->size - right_len;
        } else {
            memcpy(base + el_siz * cursize, base, el_siz * left_len);
        }
    }
}
