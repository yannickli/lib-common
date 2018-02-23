/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
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
#include "el.h"
#include "core.h"

#define NEW_MOCK_MODULE(name, init_ret, shut_ret)                            \
    static int name##_initialize(void *args)                                 \
    {                                                                        \
        return init_ret;                                                     \
    }                                                                        \
    static int name##_shutdown(void)                                         \
    {                                                                        \
        return shut_ret;                                                     \
    }                                                                        \
    static module_t *name##_module

static MODULE_METHOD(PTR, DEPS_BEFORE, before);
static MODULE_METHOD(PTR, DEPS_AFTER, after);

NEW_MOCK_MODULE(mock_ic, 1, 1);
NEW_MOCK_MODULE(mock_log, 1, 1);
NEW_MOCK_MODULE(mock_platform, 1, 1);
NEW_MOCK_MODULE(mock_thr, 1, 1);


NEW_MOCK_MODULE(mod1, 1, 1);
NEW_MOCK_MODULE(mod2, 1, 4);
NEW_MOCK_MODULE(mod3, 1, 0);
NEW_MOCK_MODULE(mod4, 1, 1);
NEW_MOCK_MODULE(mod5, 1, 1);
NEW_MOCK_MODULE(mod6, 1, 0);

NEW_MOCK_MODULE(modmethod1, 1, 1);
NEW_MOCK_MODULE(modmethod2, 1, 1);
NEW_MOCK_MODULE(modmethod3, 1, 1);
NEW_MOCK_MODULE(modmethod4, 1, 1);
NEW_MOCK_MODULE(modmethod5, 1, 1);
NEW_MOCK_MODULE(modmethod6, 1, 1);

int modmethod1;
int modmethod2;
int modmethod3;
int modmethod5;
int modmethod6;

static void modmethod1_ztst(data_t arg)
{
    modmethod1 = (*(int *)arg.ptr)++;
}

static void modmethod2_ztst(data_t arg)
{
    modmethod2 = (*(int *)arg.ptr)++;
}

static void modmethod3_ztst(data_t arg)
{
    modmethod3 = (*(int *)arg.ptr)++;
}

static void modmethod5_ztst(data_t arg)
{
    modmethod5 = (*(int *)arg.ptr)++;
}

static void modmethod6_ztst(data_t arg)
{
    modmethod6 = (*(int *)arg.ptr)++;
}


#undef NEW_MOCK_MODULE


static int module_arg_initialize(void * args)
{
    if (args == NULL)
        return -1;
    return *((int *)args);
}

static int module_arg_shutdown(void)
{
    return 1;
}
static module_t *module_arg_module;

#define Z_MODULE_REGISTER(name)                                              \
    (name##_module = module_register(LSTR_IMMED_V(#name), &name##_module,    \
                                     &name##_initialize,  &name##_shutdown,  \
                                     NULL, 0))
#define Z_MODULE_DEPENDS_ON(name, dep)                                       \
    module_add_dep(name##_module, LSTR_IMMED_V(#dep), &dep##_module)

/** Provide arguments in constructor. */
lstr_t *word_global;
lstr_t  provide_arg = LSTR_IMMED("HELLO");

MODULE_DECLARE(modprovide);

static int modprovide2_initialize(void *arg)
{
    return 0;
}
static int modprovide2_shutdown(void)
{
    return 0;
}
MODULE_BEGIN(modprovide2)
    MODULE_PROVIDE(modprovide, &provide_arg);
    MODULE_DEPENDS_ON(modprovide);
MODULE_END()

static int modprovide_initialize(void *arg)
{
    word_global = arg;
    return 0;
}
static int modprovide_shutdown(void)
{
    return 0;
}
MODULE_BEGIN(modprovide)
MODULE_END()

/** Dependency checks
 * ex. module_a depends on module_b and module_c
 *
 *          module_a
 *         /        \
 *     module_b    module_c
 *                    \
 *                  module_d
 *
 *
 *          module_g    module_e
 *              |           |
 *          module_h    module_f
 *                  \  /
 *                module_i
 *
 */

#define MODULE_INIT_SHUTDOWN_FUNCTIONS(mod) \
    static int mod##_initialize(void *arg)                                   \
    {                                                                        \
        return 0;                                                            \
    }                                                                        \
    static int mod##_shutdown(void)                                          \
    {                                                                        \
        return 0;                                                            \
    }

MODULE_INIT_SHUTDOWN_FUNCTIONS(module_a)
MODULE_INIT_SHUTDOWN_FUNCTIONS(module_b)
MODULE_INIT_SHUTDOWN_FUNCTIONS(module_c)
MODULE_INIT_SHUTDOWN_FUNCTIONS(module_d)
MODULE_INIT_SHUTDOWN_FUNCTIONS(module_e)
MODULE_INIT_SHUTDOWN_FUNCTIONS(module_f)
MODULE_INIT_SHUTDOWN_FUNCTIONS(module_g)
MODULE_INIT_SHUTDOWN_FUNCTIONS(module_h)
MODULE_INIT_SHUTDOWN_FUNCTIONS(module_i)

#undef MODULE_INIT_SHUTDOWN_FUNCTIONS

static MODULE_BEGIN(module_i)
MODULE_END()

static MODULE_BEGIN(module_h)
    MODULE_DEPENDS_ON(module_i);
MODULE_END()

static MODULE_BEGIN(module_g)
    MODULE_DEPENDS_ON(module_h);
MODULE_END()

static MODULE_BEGIN(module_f)
    MODULE_DEPENDS_ON(module_i);
MODULE_END()

static MODULE_BEGIN(module_e)
    MODULE_DEPENDS_ON(module_f);
MODULE_END()

static MODULE_BEGIN(module_d)
MODULE_END()

static MODULE_BEGIN(module_c)
    MODULE_DEPENDS_ON(module_d);
MODULE_END()

static MODULE_BEGIN(module_b)
MODULE_END()

static MODULE_BEGIN(module_a)
    MODULE_DEPENDS_ON(module_b);
    MODULE_DEPENDS_ON(module_c);
MODULE_END()

Z_GROUP_EXPORT(module)
{
    Z_TEST(basic,  "basic registering require shutdown") {
        /*         platform
         *        /   |    \
         *       /    |     \
         *      ic   thr    log
         */

        #define U_T_R "Unable to register"
        Z_ASSERT_P(Z_MODULE_REGISTER(mock_ic), U_T_R"mock_ic");
        Z_ASSERT_P(Z_MODULE_REGISTER(mock_platform), U_T_R"mock_platform");
        Z_MODULE_DEPENDS_ON(mock_platform, mock_thr);
        Z_MODULE_DEPENDS_ON(mock_platform, mock_log);
        Z_MODULE_DEPENDS_ON(mock_platform, mock_ic);

        Z_ASSERT_P(Z_MODULE_REGISTER(mock_thr), U_T_R"mock_thr");
        Z_ASSERT_P(Z_MODULE_REGISTER(mock_log), U_T_R"mock_log");
        Z_ASSERT_NULL(module_register(LSTR_IMMED_V("mock_log"),
                                      &mock_log_module,
                                      &mock_log_initialize,
                                      &mock_log_shutdown, NULL, 0));
        #undef U_T_R

        MODULE_REQUIRE(mock_log);
        MODULE_REQUIRE(mock_thr);
        MODULE_REQUIRE(mock_ic);
        MODULE_REQUIRE(mock_platform);
        Z_ASSERT(MODULE_IS_LOADED(mock_log));
        Z_ASSERT(MODULE_IS_LOADED(mock_thr));
        Z_ASSERT(MODULE_IS_LOADED(mock_ic));
        Z_ASSERT(MODULE_IS_LOADED(mock_platform));

        MODULE_RELEASE(mock_platform);
        Z_ASSERT(MODULE_IS_LOADED(mock_log));
        Z_ASSERT(MODULE_IS_LOADED(mock_thr));
        Z_ASSERT(MODULE_IS_LOADED(mock_ic));
        Z_ASSERT(!MODULE_IS_LOADED(mock_platform),
                 "mock_platform should be shutdown");

        MODULE_RELEASE(mock_log);
        Z_ASSERT(!MODULE_IS_LOADED(mock_log));
        Z_ASSERT(MODULE_IS_LOADED(mock_thr));
        Z_ASSERT(MODULE_IS_LOADED(mock_ic));
        Z_ASSERT(!MODULE_IS_LOADED(mock_platform),
                 "mock_platform should be shutdown");
        MODULE_RELEASE(mock_thr);
        Z_ASSERT(!MODULE_IS_LOADED(mock_log));
        Z_ASSERT(!MODULE_IS_LOADED(mock_thr));
        Z_ASSERT(MODULE_IS_LOADED(mock_ic));
        Z_ASSERT(!MODULE_IS_LOADED(mock_platform),
                 "mock_platform should be shutdown");
        MODULE_RELEASE(mock_ic);


        Z_ASSERT(!MODULE_IS_LOADED(mock_log), "mock_log should be shutdown");
        Z_ASSERT(!MODULE_IS_LOADED(mock_ic), "mock_ic should be shutdown");
        Z_ASSERT(!MODULE_IS_LOADED(mock_thr), "mock_thr should be shutdown");
        Z_ASSERT(!MODULE_IS_LOADED(mock_platform),
                 "mock_platform should be shutdown");
    } Z_TEST_END;

    Z_TEST(basic2,  "Require submodule") {
       MODULE_REQUIRE(mock_platform);
       MODULE_REQUIRE(mock_ic);
       Z_ASSERT(MODULE_IS_LOADED(mock_ic));
       MODULE_RELEASE(mock_platform);
       Z_ASSERT(!MODULE_IS_LOADED(mock_thr));
       Z_ASSERT(!MODULE_IS_LOADED(mock_log));
       Z_ASSERT(MODULE_IS_LOADED(mock_ic));
       MODULE_RELEASE(mock_ic);
       Z_ASSERT(!MODULE_IS_LOADED(mock_ic));
    } Z_TEST_END;


    Z_TEST(use_case1,  "Use case1") {
      /*           mod1           mod6
       *         /   |   \         |
       *        /    |    \        |
       *      mod2  mod3  mod4    mod2
       *             |
       *             |
       *            mod5
       */
      Z_MODULE_REGISTER(mod1);
      Z_MODULE_DEPENDS_ON(mod1, mod2);
      Z_MODULE_DEPENDS_ON(mod1, mod3);
      Z_MODULE_DEPENDS_ON(mod1, mod4);
      Z_MODULE_REGISTER(mod2);
      Z_MODULE_REGISTER(mod3);
      Z_MODULE_DEPENDS_ON(mod3, mod5);
      Z_MODULE_REGISTER(mod4);
      Z_MODULE_REGISTER(mod5);
      Z_MODULE_REGISTER(mod6);
      Z_MODULE_DEPENDS_ON(mod6, mod2);



      /* Test 1 All init work and shutdown work */
      MODULE_REQUIRE(mod1);
      Z_ASSERT(MODULE_IS_LOADED(mod1));
      Z_ASSERT(MODULE_IS_LOADED(mod2));
      Z_ASSERT(MODULE_IS_LOADED(mod3));
      Z_ASSERT(MODULE_IS_LOADED(mod4));
      Z_ASSERT(MODULE_IS_LOADED(mod5));
      Z_ASSERT(!MODULE_IS_LOADED(mod6));
      MODULE_REQUIRE(mod1);
      Z_ASSERT(MODULE_IS_LOADED(mod1));
      Z_ASSERT(MODULE_IS_LOADED(mod2));
      Z_ASSERT(MODULE_IS_LOADED(mod3));
      Z_ASSERT(MODULE_IS_LOADED(mod4));
      Z_ASSERT(MODULE_IS_LOADED(mod5));
      Z_ASSERT(!MODULE_IS_LOADED(mod6));
      MODULE_REQUIRE(mod6);
      Z_ASSERT(MODULE_IS_LOADED(mod1));
      Z_ASSERT(MODULE_IS_LOADED(mod2));
      Z_ASSERT(MODULE_IS_LOADED(mod3));
      Z_ASSERT(MODULE_IS_LOADED(mod4));
      Z_ASSERT(MODULE_IS_LOADED(mod5));
      Z_ASSERT(MODULE_IS_LOADED(mod6));
      MODULE_REQUIRE(mod3);
      Z_ASSERT(MODULE_IS_LOADED(mod1));
      Z_ASSERT(MODULE_IS_LOADED(mod2));
      Z_ASSERT(MODULE_IS_LOADED(mod3));
      Z_ASSERT(MODULE_IS_LOADED(mod4));
      Z_ASSERT(MODULE_IS_LOADED(mod5));
      Z_ASSERT(MODULE_IS_LOADED(mod6));

      MODULE_RELEASE(mod3);
      Z_ASSERT(MODULE_IS_LOADED(mod1));
      Z_ASSERT(MODULE_IS_LOADED(mod2));
      Z_ASSERT(MODULE_IS_LOADED(mod3));
      Z_ASSERT(MODULE_IS_LOADED(mod4));
      Z_ASSERT(MODULE_IS_LOADED(mod5));
      Z_ASSERT(MODULE_IS_LOADED(mod6));
      MODULE_RELEASE(mod1);
      Z_ASSERT(MODULE_IS_LOADED(mod1));
      Z_ASSERT(MODULE_IS_LOADED(mod2));
      Z_ASSERT(MODULE_IS_LOADED(mod3));
      Z_ASSERT(MODULE_IS_LOADED(mod4));
      Z_ASSERT(MODULE_IS_LOADED(mod5));
      Z_ASSERT(MODULE_IS_LOADED(mod6));
      MODULE_RELEASE(mod1);
      Z_ASSERT(!MODULE_IS_LOADED(mod1));
      Z_ASSERT(MODULE_IS_LOADED(mod2));
      Z_ASSERT(!MODULE_IS_LOADED(mod3));
      Z_ASSERT(!MODULE_IS_LOADED(mod4));
      Z_ASSERT(!MODULE_IS_LOADED(mod5));
      Z_ASSERT(MODULE_IS_LOADED(mod6));
      MODULE_RELEASE(mod6);
      Z_ASSERT(!MODULE_IS_LOADED(mod1));
      Z_ASSERT(!MODULE_IS_LOADED(mod2));
      Z_ASSERT(!MODULE_IS_LOADED(mod3));
      Z_ASSERT(!MODULE_IS_LOADED(mod4));
      Z_ASSERT(!MODULE_IS_LOADED(mod5));
      Z_ASSERT(!MODULE_IS_LOADED(mod6));

    } Z_TEST_END;


    Z_TEST(provide,  "Provide") {
        int a = 4;

        Z_MODULE_REGISTER(module_arg);
        MODULE_PROVIDE(module_arg, &a);
        MODULE_PROVIDE(module_arg, &a);
        MODULE_REQUIRE(module_arg);
        Z_ASSERT(MODULE_IS_LOADED(module_arg));
        MODULE_RELEASE(module_arg);
    } Z_TEST_END;

    Z_TEST(provide_constructor, "provide constructor") {
        MODULE_REQUIRE(modprovide2);
        Z_ASSERT_LSTREQUAL(*word_global, provide_arg);
        MODULE_RELEASE(modprovide2);
    } Z_TEST_END;

    Z_TEST(method, "Methods") {
        int val;

        Z_MODULE_REGISTER(modmethod1);
        Z_MODULE_DEPENDS_ON(modmethod1, modmethod2);
        module_implement_method(modmethod1_module, &after_method,
                                &modmethod1_ztst);
        module_implement_method(modmethod1_module, &before_method,
                                &modmethod1_ztst);

        Z_MODULE_REGISTER(modmethod2);
        Z_MODULE_DEPENDS_ON(modmethod2, modmethod3);
        module_implement_method(modmethod2_module, &after_method,
                                &modmethod2_ztst);
        module_implement_method(modmethod2_module, &before_method,
                                &modmethod2_ztst);

        Z_MODULE_REGISTER(modmethod3);
        Z_MODULE_DEPENDS_ON(modmethod3, modmethod4);
        module_implement_method(modmethod3_module, &after_method,
                                &modmethod3_ztst);
        module_implement_method(modmethod3_module, &before_method,
                                &modmethod3_ztst);

        Z_MODULE_REGISTER(modmethod4);
        Z_MODULE_DEPENDS_ON(modmethod4, modmethod5);
        Z_MODULE_REGISTER(modmethod5);
        module_implement_method(modmethod5_module, &after_method,
                                &modmethod5_ztst);
        module_implement_method(modmethod5_module, &before_method,
                                &modmethod5_ztst);

        Z_MODULE_REGISTER(modmethod6);
        Z_MODULE_DEPENDS_ON(modmethod6, modmethod5);
        module_implement_method(modmethod6_module, &after_method,
                                &modmethod6_ztst);
        module_implement_method(modmethod6_module, &before_method,
                                &modmethod6_ztst);

        val = 1;
        modmethod1 = modmethod2 = modmethod3 = modmethod5 = modmethod6 = 0;
        MODULE_METHOD_RUN_PTR(after, &val);
        Z_ASSERT_ZERO(modmethod1);
        Z_ASSERT_ZERO(modmethod2);
        Z_ASSERT_ZERO(modmethod3);
        Z_ASSERT_ZERO(modmethod5);
        Z_ASSERT_ZERO(modmethod6);
        Z_ASSERT_EQ(val, 1);

        MODULE_REQUIRE(modmethod1);

        val = 1;
        modmethod1 = modmethod2 = modmethod3 = modmethod5 = modmethod6 = 0;
        MODULE_METHOD_RUN_PTR(after, &val);
        Z_ASSERT_EQ(modmethod1, 1);
        Z_ASSERT_EQ(modmethod2, 2);
        Z_ASSERT_EQ(modmethod3, 3);
        Z_ASSERT_EQ(modmethod5, 4);
        Z_ASSERT_ZERO(modmethod6);
        Z_ASSERT_EQ(val, 5);

        val = 1;
        modmethod1 = modmethod2 = modmethod3 = modmethod5 = modmethod6 = 0;
        MODULE_METHOD_RUN_PTR(before, &val);
        Z_ASSERT_EQ(modmethod1, 4);
        Z_ASSERT_EQ(modmethod2, 3);
        Z_ASSERT_EQ(modmethod3, 2);
        Z_ASSERT_EQ(modmethod5, 1);
        Z_ASSERT_ZERO(modmethod6);
        Z_ASSERT_EQ(val, 5);

        MODULE_REQUIRE(modmethod6);

        val = 1;
        modmethod1 = modmethod2 = modmethod3 = modmethod5 = modmethod6 = 0;
        MODULE_METHOD_RUN_PTR(after, &val);
        Z_ASSERT_LT(modmethod1, modmethod2);
        Z_ASSERT_LT(modmethod2, modmethod3);
        Z_ASSERT_LT(modmethod3, modmethod5);
        Z_ASSERT_LT(modmethod6, modmethod5);
        Z_ASSERT(modmethod1);
        Z_ASSERT(modmethod6);
        Z_ASSERT_EQ(val, 6);

        val = 1;
        modmethod1 = modmethod2 = modmethod3 = modmethod5 = modmethod6 = 0;
        MODULE_METHOD_RUN_PTR(before, &val);
        Z_ASSERT_GT(modmethod1, modmethod2);
        Z_ASSERT_GT(modmethod2, modmethod3);
        Z_ASSERT_GT(modmethod3, modmethod5);
        Z_ASSERT_GT(modmethod6, modmethod5);
        Z_ASSERT(modmethod5);
        Z_ASSERT_EQ(val, 6);


        MODULE_RELEASE(modmethod6);
        MODULE_RELEASE(modmethod1);

        val = 1;
        modmethod1 = modmethod2 = modmethod3 = modmethod5 = modmethod6 = 0;
        MODULE_METHOD_RUN_PTR(after, &val);
        Z_ASSERT_ZERO(modmethod1);
        Z_ASSERT_ZERO(modmethod2);
        Z_ASSERT_ZERO(modmethod3);
        Z_ASSERT_ZERO(modmethod5);
        Z_ASSERT_ZERO(modmethod6);
        Z_ASSERT_EQ(val, 1);
    } Z_TEST_END;

    Z_TEST(dependency, "Modules dependency check") {
        module_t* liste1[] = {module_a_module, module_e_module};
        module_t* liste2[] = {module_a_module, module_e_module,
                              module_g_module};
        module_t* liste3[] = {module_a_module, module_e_module,
                              module_i_module};
        lstr_t collision;

        Z_ASSERT_N(module_check_no_dependencies(liste1, countof(liste1),
                                                &collision));
        Z_ASSERT_N(module_check_no_dependencies(liste2, countof(liste2),
                                                &collision));
        Z_ASSERT_NEG(module_check_no_dependencies(liste3, countof(liste3),
                                                  &collision));
    } Z_TEST_END;
} Z_GROUP_END;
