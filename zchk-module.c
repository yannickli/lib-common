/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2013 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "core-module.h"
#include "lib-common/z.h"



#define NEW_MOCK_MODULE(name, init_ret, shut_ret)                            \
    static int name##_initialize(void *args)                                 \
    {                                                                        \
        return init_ret;                                                     \
    }                                                                        \
    static int name##_shutdown(void)                                         \
    {                                                                        \
        return shut_ret;                                                     \
    }


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

NEW_MOCK_MODULE(mod1_T2, 1, 0);
NEW_MOCK_MODULE(mod3_T2, (-2), 1);

NEW_MOCK_MODULE(mod1_T3, 1, 1);
NEW_MOCK_MODULE(mod3_T3, 1, (-4));
NEW_MOCK_MODULE(mod4_T3, (-1), 1);

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
        return F_NOT_INITIALIZE;
    return *((int *)args);
}

static int module_arg_shutdown(void)
{
    return 1;
}


Z_GROUP_EXPORT(module)
{
    Z_TEST(basic,  "basic registering require shutdown") {
        /*         platform
         *        /   |    \
         *       /    |     \
         *      ic   thr    log
         */

        #define U_T_R "Unable to register"
        Z_ASSERT_EQ(MODULE_REGISTER(mock_ic, NULL), F_REGISTER,
                    U_T_R"mock_ic");
        Z_ASSERT_EQ(MODULE_REGISTER(mock_platform, NULL, "mock_thr",
                                    "mock_log", "mock_ic")
                    , F_REGISTER, U_T_R"mock_platform");
        Z_ASSERT_EQ(MODULE_REGISTER(mock_thr, NULL), F_REGISTER,
                    U_T_R"mock_thr");
        Z_ASSERT_EQ(MODULE_REGISTER(mock_log, NULL), F_REGISTER,
                    U_T_R"mock_log");
        Z_ASSERT_EQ(MODULE_REGISTER(mock_log, NULL), F_ALREADY_REGISTERED);
        #undef U_T_R



        #define U_T_I "Unable to initialize"
        Z_ASSERT_EQ(MODULE_REQUIRE(mock_log), F_INITIALIZE, U_T_I"mock_log");
        Z_ASSERT_EQ(MODULE_REQUIRE(mock_thr), F_INITIALIZE, U_T_I"mock_thr");
        Z_ASSERT_EQ(MODULE_REQUIRE(mock_ic), F_INITIALIZE, U_T_I"mock_ic");
        Z_ASSERT_EQ(MODULE_REQUIRE(mock_platform), F_INITIALIZE,
                    U_T_I"mock_platform");
        #undef U_T_I

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
       MODULE_REGISTER(mod1, NULL, "mod2", "mod3", "mod4");
       MODULE_REGISTER(mod2, NULL);
       MODULE_REGISTER(mod3, NULL, "mod5");
       MODULE_REGISTER(mod4, NULL);
       MODULE_REGISTER(mod5, NULL);
       MODULE_REGISTER(mod6, NULL, "mod2");



       /* Test 1 All init work and shutdown work */
       Z_ASSERT_EQ(MODULE_REQUIRE(mod1), F_INITIALIZE);
       Z_ASSERT_EQ(MODULE_REQUIRE(mod1), F_INITIALIZE);
       Z_ASSERT_EQ(MODULE_REQUIRE(mod6), F_INITIALIZE);
       Z_ASSERT_EQ(MODULE_REQUIRE(mod3), F_INITIALIZE);
       Z_ASSERT(MODULE_IS_LOADED(mod5));
       Z_ASSERT_EQ(MODULE_RELEASE(mod3), F_RELEASED);
       Z_ASSERT_EQ(MODULE_RELEASE(mod1),F_RELEASED);
       Z_ASSERT_EQ(MODULE_RELEASE(mod1), F_SHUTDOWN);
       Z_ASSERT(MODULE_IS_LOADED(mod2));
       MODULE_RELEASE(mod6);
       Z_ASSERT(!MODULE_IS_LOADED(mod2));


       /* Test 2 Module 3 fail init */
       MODULE_REGISTER(mod1_T2, NULL,"mod2","mod3_T2","mod4");
       MODULE_REGISTER(mod3_T2, NULL,"mod5");

       Z_ASSERT_EQ(MODULE_REQUIRE(mod1_T2),F_NOT_INITIALIZE);
       Z_ASSERT(!MODULE_IS_LOADED(mod1_T2));
       Z_ASSERT(!MODULE_IS_LOADED(mod3_T2));
       Z_ASSERT(!MODULE_IS_LOADED(mod5));
       Z_ASSERT(!MODULE_IS_LOADED(mod2));
       Z_ASSERT_EQ(MODULE_REQUIRE(mod3_T2),F_NOT_INITIALIZE);


       /** Test 3 Module 3 fail shutdown
        *         Module 4 fail init
        */
       MODULE_REGISTER(mod1_T3, NULL,"mod2","mod3_T3","mod4_T3");
       MODULE_REGISTER(mod3_T3, NULL,"mod5");
       MODULE_REGISTER(mod4_T3, NULL);

       Z_ASSERT_EQ(MODULE_REQUIRE(mod1_T3),F_NOT_INIT_AND_SHUT);
       Z_ASSERT(!MODULE_IS_LOADED(mod5));
       Z_ASSERT(!MODULE_IS_LOADED(mod4_T3));
       Z_ASSERT(!MODULE_IS_LOADED(mod3_T3));
       Z_ASSERT(!MODULE_IS_LOADED(mod3));

     } Z_TEST_END;


     Z_TEST(provide,  "Provide") {
         int *a = p_new(int, 1);
         *a = 4;

         MODULE_REGISTER(module_arg, NULL);
         Z_ASSERT_EQ(MODULE_REQUIRE(module_arg), F_NOT_INITIALIZE);

         MODULE_PROVIDE(module_arg, (void *)a);

         Z_ASSERT_EQ(MODULE_REQUIRE(module_arg), F_INITIALIZE);

         MODULE_RELEASE(module_arg);

         p_delete(&a);
     } Z_TEST_END;

     Z_TEST(onterm, "On term") {
         t_scope;
         el_data_t param3;
         el_t *param1 = t_new(el_t, 2 *3);

         param3.u32 = 42;
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

         MODULE_REGISTER(modterm1, modterm1_on_term, "modterm2");
         MODULE_REGISTER(modterm2, modterm2_on_term, "modterm3");
         MODULE_REGISTER(modterm3, modterm3_on_term, "modterm4");
         MODULE_REGISTER(modterm4, NULL, "modterm5");
         MODULE_REGISTER(modterm5, modterm5_on_term);

         MODULE_REQUIRE(modterm1);

         module_on_term(*param1, SIGINT, param3);
         Z_ASSERT_EQ(modterm1, 1);
         Z_ASSERT_EQ(modterm2, 2);
         Z_ASSERT_EQ(modterm3, 3);
         Z_ASSERT_EQ(modterm5, 5);

         module_on_term(*param1, SIGINT, param3);
         Z_ASSERT_EQ(modterm1, 2);
         Z_ASSERT_EQ(modterm2, 4);
         Z_ASSERT_EQ(modterm3, 6);
         Z_ASSERT_EQ(modterm5, 10);

         MODULE_RELEASE(modterm1);
     } Z_TEST_END;

} Z_GROUP_END;
