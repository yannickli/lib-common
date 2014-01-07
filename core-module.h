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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_MODULE_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_MODULE_H

/* {{{ Types */

/** Opaque type that defines a module.
 */
typedef struct module_t module_t;

/* }}} */
/* {{{ Module creation */

/** Declare a module.
 *
 * This macro declares a module variable.
 */
#define MODULE_DECLARE(name)  extern module_t *name##_module


/** Begin the definition of a module.
 *
 * This begin a section of code that can contain the description of a module.
 * The section can currently contain the following descriptions:
 * - \ref MODULE_DEPENDS_ON to add a dependency.
 *
 * The macro can also take a variadic list of dependencies as strings, but
 * this usage is discouraged since it does not add compile-time dependencies
 * between compilation units. Using \ref MODULE_DEPENDS_ON should be the
 * prefered solution to add a new dependence.
 *
 * The section must be closed by calling \ref MODULE_END().
 */
#define MODULE_BEGIN(name, on_term, ...)                                     \
    __attr_section("intersec", "module")                                     \
    module_t *name##_module;                                                 \
                                                                             \
    static __attribute__((constructor))                                      \
    void __##name##_module_register(void) {                                  \
        __unused__                                                           \
        module_t *__mod = name##_module = MODULE_REGISTER(name, on_term,     \
                                                          ##__VA_ARGS__);

/** Macro to end the definition of a module.
 *
 * \see MODULE_BEGIN
 */
#define MODULE_END()  }

/** Add a dependence on another module.
 *
 * This macro can only be used in a MODULE_BEGIN/MODULE_END block of code. It
 * declares a dependence from the current module on \p dep.
 */
#define MODULE_DEPENDS_ON(dep)  \
    module_add_dep(__mod, LSTR_IMMED_V(#dep), &dep##_module)

/* {{{ Old API */

/** \brief Macro for registering a module
 *              module1
 *             /       \
 *         module2   module3
 *
 *
 *  Prototype of the module:
 *           int module1_initialize(void *); Return >= 0 if success
 *           int module1_shutdown(void);     Return >= 0 if success
 *           void module1_on_term(int);
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
 *
 *  You can use \ref MODULE to perform automatic registration of a module.
 */

#define MODULE_REGISTER(name, on_term, ...)                                  \
    ({ const char *__##name##_dependencies[] = {NULL, "log", ##__VA_ARGS__ };\
       module_register(LSTR_IMMED_V(#name), &name##_initialize,              \
                       on_term, &name##_shutdown,                            \
                       __##name##_dependencies + 1,                          \
                       countof(__##name##_dependencies) - 1); })


/** Macro to perform automatical module registration.
 *
 * This macro declares a function that will automatically register a module at
 * binary/library initialization. It takes the same arguments as \ref
 * MODULE_REGISTER.
 */
#define MODULE(name, on_term, ...)                                           \
    MODULE_BEGIN(name, on_term, ##__VA_ARGS__)                               \
    MODULE_END()

/* }}} */
/* {{{ Low-level API */

/** \brief Register a module
 *
 *  @param name Name of the module
 *  @param initialize Pointer to the function that initialize the module
 *  @param shutdown Pointer to the function that shutdown the module
 *  @param dependencies list of modules
 *  @param nb_dependencies number of dependent modules
 *
 *
 *  @return The newly registered module in case of success.
 */
__leaf
module_t *module_register(lstr_t name, int (*constructor)(void *),
                          void (*on_term)(int signo), int (*destructor)(void),
                          const char *dependencies[], int nb_dependencies);

__attr_nonnull__((1))
void module_add_dep(module_t *mod, lstr_t dep, module_t **dep_ptr);

/* }}} */
/* }}} */
/* {{{ Module management */

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
    module_provide(name##_module, argument)


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
 *             - module_require will throw a logger_fatal
 *
 */
#define MODULE_REQUIRE(name)  module_require(name##_module, NULL)

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

#define MODULE_RELEASE(name)  module_release(name##_module)


#define MODULE_IS_LOADED(name)  module_is_loaded(name##_module)


/* {{{ Low-level API */

#define F_INITIALIZE  1

/** \brief Require a module (initialization)
 *
 *  Two stepts  : - Require dependent modules
 *                - Initialize the module
 *
 *  If one of the required modules does not initialize, the function
 *  will throw a logger_fatal.
 *
 *  @param mod Name of the module to initialize
 *  @param required_by - Module that requires \p mod to be initialized
 *                     - NULL -> No parent modules
 *
 *
 *  @return
 *       F_INITIALIZE
 */
__attr_nonnull__((1))
void module_require(module_t *mod, module_t *required_by);


#define F_RELEASED  3
#define F_SHUTDOWN  1
#define F_NOTIFIED  2
#define F_NOT_SHUTDOWN  (-1) /* One of the module did not shutdown */
#define F_UNAUTHORIZE_RELEASE (-2)

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
 *  @param mod The module to shutdown
 *
 *  @return
 *       F_SHUTDOWN
 *       F_NOT_SHUTDOWN
 *       F_NOTIFIED
 */
__attr_nonnull__((1))
int module_shutdown(module_t *mod);


/** \brief Release a module
 *
 *  assert if you try to release a module that has been automatically loaded
 *
 *  @param mod Module to shutdown
 *
 *  @return
 *       F_SHUTDOWN
 *       F_NOT_SHUTDOWN
 *       F_RELEASED
 */
__attr_nonnull__((1))
int module_release(module_t *mod);

/** \brief On term all modules
 *
 *  Call the module_on_term if it exists of all modules
 */
void module_on_term(int signo);


__attr_nonnull__((1, 2))
void module_provide(module_t *mod, void *argument);

/** true if module is loaded (AUTO_REQ || MANU_REQ) */
__attr_nonnull__((1))
bool module_is_loaded(const module_t *mod);

/* }}} */

#endif
