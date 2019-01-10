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

#include "z.h"
#include "core.h"

/*{{{1 Memory Pool Macros */

/* The purpose of these checks is to make sure that:
 *     1. The syntax of these helpers is not broken;
 *     2. They can run at least once without crashing immediately.
 *
 * The purpose of these checks is *not* to fully check any allocator.
 */

typedef struct {
    int foo;
    int tab[];
} z_mp_test_t;

Z_GROUP_EXPORT(mem_pool_macros) {
    Z_TEST(t_pool, "t_pool: helpers macros") {
        t_scope;
        __unused__ int *p;
        z_mp_test_t *t;
        __unused__ char *s;

        p = t_new_raw(int, 42);
        p = t_new(int, 42);

        p = t_new_extra(int, 16);
        p = t_new_extra_raw(int, 16);

        t = t_new_extra_field_raw(z_mp_test_t, tab, 42);
        t = t_new_extra_field(z_mp_test_t, tab, 42);

        t = t_dup(t, 1);
        s = t_dupz("toto", 4);

        Z_ASSERT(true, "execution OK");
    } Z_TEST_END

    Z_TEST(r_pool, "r_pool: helpers macros") {
        const void *frame;
        __unused__ int *p;
        z_mp_test_t *t;
        __unused__ char *s;

        frame = r_newframe();

        p = r_new_raw(int, 42);
        p = r_new(int, 42);

        p = r_new_extra(int, 16);

        t = r_new(z_mp_test_t, 256);
        t = r_dup(t, 256);
        s = r_dupz("toto", 4);

        r_release(frame);

        Z_ASSERT(true, "execution OK");
    } Z_TEST_END

    Z_TEST(mem_libc, "mem_libc pool: helpers macros") {
        int *p;
        z_mp_test_t *t;
        char *s;

        p = p_new_raw(int, 42);
        p_delete(&p);
        p = p_new(int, 42);
        p_realloc(&p, 64);
        p_realloc0(&p, 64, 512);
        p_delete(&p);

        p = p_new_extra(int, 16);
        p_delete(&p);
        p = p_new_extra_raw(int, 16);
        p_realloc_extra(&p, 42);
        p_realloc0_extra(&p, 42, 21);
        p_delete(&p);

        t = p_new_extra_field(z_mp_test_t, tab, 42);
        p_delete(&t);
        t = p_new_extra_field_raw(z_mp_test_t, tab, 42);
        p_realloc_extra_field(&t, tab, 128);
        p_realloc0_extra_field(&t, tab, 128, 256);
        p_delete(&t);

        s = p_dup("toto", 5);
        p_delete(&s);
        s = p_dupz("toto", 4);
        p_delete(&s);
        s = p_strdup("toto");
        p_delete(&s);

        Z_ASSERT(true, "execution OK");
    } Z_TEST_END
} Z_GROUP_END

/*}}}1*/
/*{{{1 Memstack */

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

/*}}}1*/
/*{{{1 Memring */

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

/*}}}1*/
