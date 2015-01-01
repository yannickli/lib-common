/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2015 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "z.h"

/*{{{1 FIFO Pool */

Z_GROUP_EXPORT(fifo)
{
    Z_TEST(fifo_pool, "fifo_pool:allocate an amount near pool page size") {
        int page_size = 1 << 19;
        mem_pool_t *pool = mem_fifo_pool_new(page_size);
        char vtest[2 * page_size];
        char *v;

        Z_ASSERT(pool);
        p_clear(&vtest, 1);

        v = mp_new(pool, char, page_size - 20);
        Z_ASSERT_ZERO(memcmp(v, vtest, page_size - 20));
        mp_delete(pool, &v);

        v = mp_new(pool, char, page_size);
        Z_ASSERT_ZERO(memcmp(v, vtest, page_size));
        mp_delete(pool, &v);

        v = mp_new(pool, char, page_size + 20);
        Z_ASSERT_ZERO(memcmp(v, vtest, page_size + 20));
        mp_delete(pool, &v);

        mem_fifo_pool_delete(&pool);
    } Z_TEST_END
} Z_GROUP_END

/*1}}}*/
