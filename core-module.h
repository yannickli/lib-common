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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_MODULE_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_MODULE_H

/*{{{ Macros */

/** \brief Macro for registering a module
 *              module1
 *             /       \
 *         module2   module3
 *
 *
 *  Prototype of the module:
 *           int module1_initialize(void *); Return >= 0 if success
 *           int module1_shutdown(void);     Return >= 0 if success
 *           int module1_on_term(int);       Return >= 0 if success
 *
 *  Arguments:
 *        + name of the module
 *        + callback to the module_on_term function (NULL if none)
 *        + list of dependencies (between "")
 *
 *  Use:
 *      MODULE_REGISTER(module1, module1_on_term, "module2", "module3");
 *      MODULE_REGISTER(module2, NULL);
 *      MODULE_REGISTER(module3, NULL);
 */

#define MODULE_REGISTER(name, on_term, ...)                                  \
    module_register(LSTR_IMMED_V(#name), &name##_initialize, on_term,        \
                    &name##_shutdown, ##__VA_ARGS__, NULL)

/** \brief Provide an argument for module constructor
 *  Use:
 *      MODULE_PROVIDE(module, (void *)arg)
 *
 *  Your initialization function (alias constructor function) should
 *  return a negative number if it needs an argument and this argument
 *  is not provided (by default NULL)
 *  Exe:
 *     int module_initialize(void *arg) {
 *         if (arg == NULL)
 *            return F_NOT_INITIALIZE; (or assert)
 *       ...
 *     }
 *
 */

#define MODULE_PROVIDE(name, argument)                                       \
    module_provide(LSTR_IMMED_V(#name), argument)


/** \brief Macro for requiring a module
 *   Use:
 *     MODULE_REQUIRE(module1);
 *
 *              module1
 *             /       \
 *         module2   module3
 *
 *   Behavior:
 *      + If every thing works
 *             - module1, module2, module3 initialized
 *             - module1 will have state MANU_REQ
 *             - module2, module3 will have state AUTO_REQ
 *             - MODULE_REQUIRE will return F_INITIALIZE
 *       + If module3 fail to initialize
 *              - module2 will be release
 *              - module3 will have state FAIL_REQ
 *              - module1 will have state FAIL_REQ and won't be initialized
 *              - MODULE_REQUIRE will return F_NOT_INITIALIZE
 *     + If module3 fail to initialize and module2 fail to shutdown
 *             - module2 will have state FAIL_SHUT
 *             - module3 will have state FAIL_REQ
 *             - module1 will have state FAIL_REQ_AND_SHUT
 *             - MODULE_REQUIRE will return F_NOT_INIT_AND_SHUT
 *
 */

#define MODULE_REQUIRE(name)                                                 \
    ({  int _res = module_require(LSTR_IMMED_V(#name), LSTR_NULL_V);         \
        int _shut;                                                           \
        if(_res < 0){                                                        \
            _shut = module_shutdown(LSTR_IMMED_V(#name));                    \
            if(_shut < 0)                                                    \
                _res = F_NOT_INIT_AND_SHUT;                                  \
        }                                                                    \
        _res;                                                                \
    })

/** \brief Macro for releasing a module
 *  Use:
 *    MODULE_RELEASE(module);
 *
 *  Behavior:
 *     + If you try to RELEASE a module that has not been manually initialized
 *       the program will assert
 *       In other words only RELEASE modules that has been require with REQUIRE
 *     + For returns value see module_release and module_shutdown
 *
 */

#define MODULE_RELEASE(name)                                                 \
    module_release(LSTR_IMMED_V(#name))


#define MODULE_IS_LOADED(name)  module_is_loaded(LSTR_IMMED_V(#name))


/*}}} */
/*{{{ module functions */

/** \brief Register a module
 *
 *  @param name Name of the module
 *  @param initialize Pointer to the function that initialize the module
 *  @param shutdown Pointer to the function that shutdown the module
 *  @param ... list of the dependent module
 *
 *
 *  @return
 *       F_REGISTER
 *       F_ALREADY_REGISTER
 */

#define F_REGISTER  1
#define F_ALREADY_REGISTERED  (-1)

int module_register(lstr_t name, int (*constructor)(void *),
                    void (*on_term)(int signo),
                    int (*destructor)(void), ...);


/** \brief Require a module (initialization)
 *
 *  Two stepts  : - Require dependent modules
 *                - Initialize the module
 *
 *  If one of the required modules does not initialize, the function
 *  return F_NOT_INITIALIZE, the module will not be initilize but
 *  some of the dependent might be: run shutdown(name) to shutdown
 *  the required modules that have been initialize. (cf macro MODULEREQUIRE())
 *
 *  @param name Name of the module to initialize
 *  @param required_by - lstr_t    -> Name of a module that requires
 *                                     "name" to be initialized
 *                     - LSTR_NULL -> No parent modules
 *
 *
 *  @return
 *       F_INITIALIZE
 *       F_NOT_INITIALIZE
 */

#define F_INITIALIZE  1
#define F_NOT_INITIALIZE  (-1)
#define F_NOT_INIT_AND_SHUT  (-2)

int module_require(lstr_t name, lstr_t required_by);



/** \brief Shutdown a module
 *
 *  Two stepts  :   - Shutdown the module.
 *                  - Notify dependent modules that it has been shutdown
 *                    if the dependent modules don't have any other parent
 *                    modules and they have been automatically initialize
 *                    they will shutdown
 *
 *  If the module is not able to shutdown (destructor returns a negative
 *  number), module state change to FAIL_SHUT but we considered as shutdown
 *  and notify dependent modules.
 *
 *
 *  @param name Name of the module to shutdown
 *
 *  @return
 *       F_SHUTDOWN
 *       F_NOT_SHUTDOWN
 *       F_NOTIFIED
 */

#define F_RELEASED  3
#define F_SHUTDOWN  1
#define F_NOTIFIED  2
#define F_NOT_SHUTDOWN  (-1) /* One of the module did not shutdown */
#define F_UNAUTHORIZE_RELEASE (-2)


int module_shutdown(lstr_t name);


/** \brief Release a module
 *
 *  assert if you try to release a module that has been automatically loaded
 *
 *  @param name Name of the module to shutdown
 *
 *  @return
 *       F_SHUTDOWN
 *       F_NOT_SHUTDOWN
 *       F_RELEASED
 */


int module_release(lstr_t name);

/** \brief On term all modules
 *
 *  Call the module_on_term if it exists of all modules
 */
void module_on_term(int signo);


void module_provide(lstr_t name, void *argument);

/** true if module is loaded (AUTO_REQ || MANU_REQ) */
bool module_is_loaded(lstr_t name);

/*}}} */

#endif
