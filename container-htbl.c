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

#include "container.h"
#include "hash.h"

void htbl_init(generic_htbl *t, int size)
{
    t->size      = size;
    t->len       = 0;
    t->ghosts    = 0;
    t->setbits   = p_new(unsigned, BITS_TO_ARRAY_LEN(unsigned, size));
    t->ghostbits = p_new(unsigned, BITS_TO_ARRAY_LEN(unsigned, size));
}

void htbl_wipe(generic_htbl *t)
{
    p_delete(&t->tab);
    p_delete(&t->setbits);
    p_delete(&t->ghostbits);
}

void htbl_invalidate(generic_htbl *t, int pos)
{
    int next = pos + 1 == t->size ? 0 : pos + 1;

    if (TST_BIT(t->setbits, pos)) {
        RST_BIT(t->setbits, pos);
        t->len--;
        SET_BIT(t->ghostbits, pos);
        t->ghosts++;
    }
    if (TST_BIT(t->setbits, next) || TST_BIT(t->ghostbits, next))
        return;
    while (TST_BIT(t->ghostbits, pos)) {
        RST_BIT(t->ghostbits, pos);
        t->ghosts--;
        if (pos-- == 0)
            pos = t->size - 1;
    }
}

uint64_t htbl_hash_string(const void *s, int len)
{
    if (len < 0)
        len = strlen(s);
    return MAKE64(hsieh_hash((const byte *)s, len), len);
}

bool htbl_keyequal(uint64_t h, const void *k1, const void *k2)
{
    return !memcmp(k1, k2, (h & INT_MAX));
}
