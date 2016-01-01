/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2016 INTERSEC SA                                   */
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

Z_GROUP_EXPORT(core_mem_stack) {
    Z_TEST(big_alloc_mean, "non regression on #39120") {
        mem_stack_pool_t sp;

        mem_stack_pool_init(&sp, 0);

        mem_stack_push(&sp);

        /* First big allocation to set a big allocation mean */
        Z_ASSERT_P(mp_new_raw(&sp.funcs, char, 50 << 20));
        /* Second big allocation to make the allocator abort */
        Z_ASSERT_P(mp_new_raw(&sp.funcs, char, 50 << 20));

        mem_stack_pop(&sp);
        mem_stack_pool_wipe(&sp);
    } Z_TEST_END
} Z_GROUP_END

Z_GROUP_EXPORT(core_mem_ring) {
    Z_TEST(big_alloc_mean, "non regression on #39120") {
        mem_pool_t *rp = mem_ring_pool_new(0);
        const void *rframe = mem_ring_newframe(rp);

        /* First big allocation to set a big allocation mean */
        Z_ASSERT_P(mp_new_raw(rp, char, 50 << 20));
        /* Second big allocation to make the allocator abort */
        Z_ASSERT_P(mp_new_raw(rp, char, 50 << 20));

        mem_ring_release(rframe);
        mem_ring_pool_delete(&rp);
    } Z_TEST_END
} Z_GROUP_END
