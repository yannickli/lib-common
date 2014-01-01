/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2014 INTERSEC SA                                   */
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

NEW_MOCK_MODULE(modterm1, 1, 1);
NEW_MOCK_MODULE(modterm2, 1, 1);
NEW_MOCK_MODULE(modterm3, 1, 1);
NEW_MOCK_MODULE(modterm4, 1, 1);
NEW_MOCK_MODULE(modterm5, 1, 1);

int modterm1;
int modterm2;
int modterm3;
int modterm5;

static void modterm1_on_term(int i)
{
    modterm1 += 1;
}

static void modterm2_on_term(int i)
{
    modterm2 += 2;
}

static void modterm3_on_term(int i)
{
    modterm3 += 3;
}

static void modterm5_on_term(int i)
{
    modterm5 += 5;
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

#define Z_MODULE_REGISTER(name, ...)  \
    (name##_module = MODULE_REGISTER(name, ##__VA_ARGS__))


Z_GROUP_EXPORT(module)
{
    Z_TEST(basic,  "basic registering require shutdown") {
        /*         platform
         *        /   |    \
         *       /    |     \
         *      ic   thr    log
         */

        #define U_T_R "Unable to register"
        Z_ASSERT_P(Z_MODULE_REGISTER(mock_ic, NULL),
                    U_T_R"mock_ic");
        Z_ASSERT_P(Z_MODULE_REGISTER(mock_platform, NULL, "mock_thr",
                                    "mock_log", "mock_ic"),
                   U_T_R"mock_platform");
        Z_ASSERT_P(Z_MODULE_REGISTER(mock_thr, NULL),
                    U_T_R"mock_thr");
        Z_ASSERT_P(Z_MODULE_REGISTER(mock_log, NULL),
                    U_T_R"mock_log");
        Z_ASSERT_NULL(MODULE_REGISTER(mock_log, NULL));
        #undef U_T_R

        MODULE_REQUIRE(mock_log);
        MODULE_REQUIRE(mock_thr);
        MODULE_REQUIRE(mock_ic);
        MODULE_REQUIRE(mock_platform);
        Z_ASSERT(MODULE_IS_LOADED(mock_log));
        Z_ASSERT(MODULE_IS_LOADED(mock_thr));
        Z_ASSERT(MODULE_IS_LOADED(mock_ic));
        Z_ASSERT(MODULE_IS_LOADED(mock_platform));

        Z_ASSERT_EQ(MODULE_RELEASE(mock_platform), F_SHUTDOWN);
        Z_ASSERT(MODULE_IS_LOADED(mock_log));
        Z_ASSERT(!MODULE_IS_LOADED(mock_platform),
                 "mock_platform should be shutdown");

        Z_ASSERT_EQ(MODULE_RELEASE(mock_log), F_SHUTDOWN);
        Z_ASSERT_EQ(MODULE_RELEASE(mock_thr), F_SHUTDOWN);
        Z_ASSERT_EQ(MODULE_RELEASE(mock_ic), F_SHUTDOWN);


        Z_ASSERT(!MODULE_IS_LOADED(mock_log), "mock_log should be shutdown");
        Z_ASSERT(!MODULE_IS_LOADED(mock_ic), "mock_ic should be shutdown");
        Z_ASSERT(!MODULE_IS_LOADED(mock_thr), "mock_thr should be shutdown");


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
       Z_MODULE_REGISTER(mod1, NULL, "mod2", "mod3", "mod4");
       Z_MODULE_REGISTER(mod2, NULL);
       Z_MODULE_REGISTER(mod3, NULL, "mod5");
       Z_MODULE_REGISTER(mod4, NULL);
       Z_MODULE_REGISTER(mod5, NULL);
       Z_MODULE_REGISTER(mod6, NULL, "mod2");



       /* Test 1 All init work and shutdown work */
       MODULE_REQUIRE(mod1);
       MODULE_REQUIRE(mod1);
       MODULE_REQUIRE(mod6);
       MODULE_REQUIRE(mod3);
       Z_ASSERT(MODULE_IS_LOADED(mod5));
       Z_ASSERT_EQ(MODULE_RELEASE(mod3), F_RELEASED);
       Z_ASSERT_EQ(MODULE_RELEASE(mod1),F_RELEASED);
       Z_ASSERT_EQ(MODULE_RELEASE(mod1), F_SHUTDOWN);
       Z_ASSERT(MODULE_IS_LOADED(mod2));
       MODULE_RELEASE(mod6);
       Z_ASSERT(!MODULE_IS_LOADED(mod2));

     } Z_TEST_END;


     Z_TEST(provide,  "Provide") {
         int *a = p_new(int, 1);
         *a = 4;

         Z_MODULE_REGISTER(module_arg, NULL);
         MODULE_PROVIDE(module_arg, (void *)a);
         MODULE_REQUIRE(module_arg);
         Z_ASSERT(MODULE_IS_LOADED(module_arg));
         MODULE_RELEASE(module_arg);

         p_delete(&a);
     } Z_TEST_END;

     Z_TEST(onterm, "On term") {
         /**       modterm1
          *           |
          *        modterm2
          *           |
          *        modterm3
          *           |
          *        modterm4
          *           |
          *        modterm5
          **/

         Z_MODULE_REGISTER(modterm1, modterm1_on_term, "modterm2");
         Z_MODULE_REGISTER(modterm2, modterm2_on_term, "modterm3");
         Z_MODULE_REGISTER(modterm3, modterm3_on_term, "modterm4");
         Z_MODULE_REGISTER(modterm4, NULL, "modterm5");
         Z_MODULE_REGISTER(modterm5, modterm5_on_term);

         MODULE_REQUIRE(modterm1);

         module_on_term(SIGINT);
         Z_ASSERT_EQ(modterm1, 1);
         Z_ASSERT_EQ(modterm2, 2);
         Z_ASSERT_EQ(modterm3, 3);
         Z_ASSERT_EQ(modterm5, 5);

         module_on_term(SIGINT);
         Z_ASSERT_EQ(modterm1, 2);
         Z_ASSERT_EQ(modterm2, 4);
         Z_ASSERT_EQ(modterm3, 6);
         Z_ASSERT_EQ(modterm5, 10);

         MODULE_RELEASE(modterm1);
     } Z_TEST_END;

} Z_GROUP_END;
