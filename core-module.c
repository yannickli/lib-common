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

#include <pthread.h>

#include "log.h"
#include "container-qvector.h"
#include "container-qhash.h"

/* {{{ Type definition */
/* {{{ methods */

static inline uint32_t module_method_hash(const qhash_t *h,
                                          const module_method_t *m)
{
    return qhash_hash_ptr(h, m);
}
static inline bool module_method_equal(const qhash_t *h,
                                       const module_method_t *m1,
                                       const module_method_t *m2)
{
    return qhash_ptr_equal(h, m1, m2);
}
qm_kptr_ckey_t(methods, module_method_t, void *,
               module_method_hash, module_method_equal);

qvector_t(methods_cb, void *);

typedef struct module_method_impl_t {
    const module_method_t *params;
    qv_t(methods_cb) callbacks;
} module_method_impl_t;

GENERIC_NEW_INIT(module_method_impl_t, module_method);
static module_method_impl_t *module_method_wipe(module_method_impl_t *method)
{
    qv_wipe(methods_cb, &method->callbacks);
    return method;
}
GENERIC_DELETE(module_method_impl_t, module_method);

qm_kptr_ckey_t(methods_impl, module_method_t, module_method_impl_t *,
               module_method_hash, module_method_equal);
static void module_build_method_all_cb(void);

/* }}} */
/* {{{ modules */

typedef enum module_state_t {
    REGISTERED   = 0,
    AUTO_REQ     = 1 << 0, /* Initialized automatically */
    MANU_REQ     = 1 << 1, /* Initialized manually */

    INITIALIZING = 1 << 2, /* In initialization */
    SHUTTING     = 1 << 3, /* Shutting down */
    FAIL_SHUT    = 1 << 4, /* Fail to shutdown */
} module_state_t;

qvector_t(module, module_t *);

struct module_t {
    lstr_t name;
    module_state_t state;
    int manu_req_count;

    /* Vector of module name */
    qv_t(lstr)   dependent_of;
    qv_t(module) required_by;
    qm_t(methods) methods;

    int (*constructor)(void *);
    int (*destructor)(void);
    void *constructor_argument;
};

static module_t *module_init(module_t *module)
{
    p_clear(module, 1);
    qm_init(methods, &module->methods);
    return module;
}
GENERIC_NEW(module_t, module);
static void module_wipe(module_t *module)
{
    lstr_wipe(&(module->name));
    qv_deep_wipe(lstr, &module->dependent_of, lstr_wipe);
    qv_wipe(module, &module->required_by);
    qm_wipe(methods, &module->methods);
}
GENERIC_DELETE(module_t, module);


qm_kptr_t(module, lstr_t, module_t *, qhash_lstr_hash, qhash_lstr_equal);

qm_kptr_t(module_arg, void, void *, qhash_hash_ptr, qhash_ptr_equal);

qm_kvec_t(module_dep, lstr_t, qh_t(ptr), qhash_lstr_hash, qhash_lstr_equal);

/* }}} */
/* }}} */

static struct module_g {
    logger_t logger;
    qm_t(module)     modules;
    qm_t(module_arg) modules_arg;
    qm_t(module_dep) module_dep_resolve;

    qm_t(methods_impl) methods;

    /* Keep track if we are currently initializing a module */
    int in_initialization;
    bool is_shutdown;
} module_g = {
#define _G module_g
    .logger = LOGGER_INIT(NULL, "module", LOG_INHERITS),
    .modules     = QM_INIT(module, _G.modules),
    .modules_arg = QM_INIT(module_arg, _G.modules_arg),
    .module_dep_resolve = QM_INIT(module_dep, _G.module_dep_resolve),
    .methods = QM_INIT(methods_impl, _G.methods),
};

/* {{{ Module Registry */

static void
set_require_type(module_t *module, module_t *required_by)
{
    if (!required_by) {
        module->state = MANU_REQ;
        module->manu_req_count++;
    } else {
        qv_append(module, &module->required_by, required_by);
        if (module->state != MANU_REQ)
            module->state = AUTO_REQ;
    }
}

module_t *module_register(lstr_t name, module_t **module,
                          int (*constructor)(void *),
                          int (*destructor)(void),
                          const char *dependencies[], int nb_dependencies)
{
    module_t *new_module;
    int pos, arg_pos, qm_pos;

    pos = qm_reserve(module, &_G.modules, &name, 0);

    if (pos & QHASH_COLLISION) {
        logger_warning(&_G.logger,
                       "%*pM has already been register", LSTR_FMT_ARG(name));
        return NULL;
    }

    new_module = module_new();
    new_module->state = REGISTERED;
    new_module->name = lstr_dupc(name);
    new_module->manu_req_count = 0;
    if ((arg_pos = qm_find(module_arg, &_G.modules_arg, module)) >= 0) {
        new_module->constructor_argument = _G.modules_arg.values[arg_pos];
    } else {
        new_module->constructor_argument = NULL;
    }
    new_module->constructor = constructor;
    new_module->destructor = destructor;

    for (int i = 0; i < nb_dependencies; i++) {
        qv_append(lstr, &new_module->dependent_of, LSTR(dependencies[i]));
    }

    qm_pos = qm_del_key(module_dep, &_G.module_dep_resolve, &name);
    if (qm_pos >= 0) {
        qh_t(ptr) *modules_dep;

        modules_dep = &_G.module_dep_resolve.values[qm_pos];
        qh_for_each_pos(ptr, qh_pos, modules_dep) {
            qv_append(lstr, &new_module->dependent_of,
                      ((module_t *)modules_dep->keys[qh_pos])->name);
        }
        qh_wipe(ptr, modules_dep);
        lstr_wipe(&_G.module_dep_resolve.keys[qm_pos]);
    }

    _G.modules.keys[pos] = &new_module->name;
    _G.modules.values[pos] = new_module;

    return new_module;
}

void module_add_dep(module_t *module, lstr_t name, lstr_t dep,
                    module_t **dep_ptr)
{
    /* XXX dep_ptr is used only to force an explicit dependency between
     * modules. This guarantees that if module is present in the binary, the
     * dependency will be present too.
     */

    if (!module) {
        int pos;
        qh_t(ptr) *dep_modules;

        pos = qm_reserve(module_dep, &_G.module_dep_resolve, &name, 0);

        if (pos & QHASH_COLLISION) {
            pos ^= QHASH_COLLISION;

            dep_modules = &_G.module_dep_resolve.values[pos];
        } else {
            dep_modules = &_G.module_dep_resolve.values[pos];
            qh_init(ptr, dep_modules);
        }
        IGNORE(expect(qh_add(ptr, dep_modules, *dep_ptr) >= 0));
        return;
    }

    assert (module->state == REGISTERED);
    qv_append(lstr, &module->dependent_of, dep);
}

void module_require(module_t *module, module_t *required_by)
{
    if (module->state == INITIALIZING) {
        logger_fatal(&_G.logger,
                     "`%*pM` has been recursively required %s%*pM",
                     LSTR_FMT_ARG(module->name), required_by ? "by " : "",
                     LSTR_FMT_ARG(required_by ? required_by->name
                                              : LSTR_NULL_V));
    }
    if (module->state == SHUTTING) {
        logger_fatal(&_G.logger,
                     "`%*pM` has been required %s%*pM while shutting down",
                     LSTR_FMT_ARG(module->name), required_by ? "by " : "",
                     LSTR_FMT_ARG(required_by ? required_by->name
                                              : LSTR_NULL_V));
    }

    _G.in_initialization++;

    if (!module_is_loaded(module)) {
        logger_trace(&_G.logger, 1, "`%*pM` has been required %s%*pM",
                     LSTR_FMT_ARG(module->name), required_by ? "by " : "",
                     LSTR_FMT_ARG(required_by ? required_by->name
                                              : LSTR_NULL_V));
    }

    if (module->state == AUTO_REQ || module->state == MANU_REQ) {
        set_require_type(module, required_by);
        _G.in_initialization--;
        return;
    }

    module->state = INITIALIZING;
    logger_trace(&_G.logger, 1, "requiring `%*pM` dependencies",
                 LSTR_FMT_ARG(module->name));

    module_build_method_all_cb();

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        module_require(qm_get(module, &_G.modules, &dep), module);
    }

    logger_trace(&_G.logger, 1, "calling `%*pM` constructor",
                 LSTR_FMT_ARG(module->name));

    if ((*module->constructor)(module->constructor_argument) >= 0) {
        set_require_type(module, required_by);
        module_build_method_all_cb();
        _G.in_initialization--;
        return;
    }
    logger_fatal(&_G.logger, "unable to initialize %*pM",
                 LSTR_FMT_ARG(module->name));
}



void module_provide(module_t **module, void *argument)
{
    if (!(*module)) {
        if (qm_add(module_arg, &_G.modules_arg, module, argument) < 0) {
            logger_warning(&_G.logger, "argument has already been provided");
        }
        return;
    }
    if ((*module)->constructor_argument) {
        logger_warning(&_G.logger, "argument for module '%*pM' has already "
                       "been provided", LSTR_FMT_ARG((*module)->name));
    }
    (*module)->constructor_argument = argument;
}

__attr_nonnull__((1))
static int module_shutdown(module_t *module);

static int notify_shutdown(module_t *module, module_t *dependence)
{
    logger_trace(&_G.logger, 2, "module '%*pM' notify shutdown to '%*pM'"
                 ": %d pending dependencies", LSTR_FMT_ARG(dependence->name),
                 LSTR_FMT_ARG(module->name), module->required_by.len);

    qv_for_each_pos(module, pos, &module->required_by) {
        if (module->required_by.tab[pos] == dependence) {
            qv_remove(module, &module->required_by, pos);
            break;
        }
    }
    if (module->required_by.len == 0 && module->state != MANU_REQ) {
        return module_shutdown(module);
    }

    return 0;
}

/** \brief Shutdown a module
 *
 *  Two steps   :   - Shutdown the module.
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
 */
__attr_nonnull__((1))
static int module_shutdown(module_t *module)
{
    int shut_dependent = 1;
    int shut_self;

    assert (module->state == MANU_REQ || module->state == AUTO_REQ);

    /* shutdown must be symmetric to require */
    module->manu_req_count = 0;

    module->state = SHUTTING;
    logger_trace(&_G.logger, 1, "shutting down `%*pM`",
                 LSTR_FMT_ARG(module->name));

    if ((shut_self = (*module->destructor)()) < 0) {
        logger_warning(&_G.logger, "unable to shutdown   %*pM",
                       LSTR_FMT_ARG(module->name));
        module->state = FAIL_SHUT;
    }

    module_build_method_all_cb();

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        int shut;

        shut = notify_shutdown(qm_get(module, &_G.modules, &dep), module);
        if (shut < 0) {
            shut_dependent = shut;
        }
    }

    if (shut_self >= 0) {
        module->state = REGISTERED;
    }

    RETHROW(shut_dependent);
    RETHROW(shut_self);
    return 0;
}

void module_release(module_t *module)
{
    if (module->manu_req_count == 0) {
        /* You are trying to manually release a module that have been spawn
         * automatically (AUTO_REQ)
         */
        logger_panic(&_G.logger, "unauthorized release for module '%*pM'",
                     LSTR_FMT_ARG(module->name));
        return;
    }

    if (module->state == MANU_REQ && module->manu_req_count > 1) {
        module->manu_req_count--;
        return;
    }

    if (module->state == MANU_REQ && module->manu_req_count == 1) {
        if (module->required_by.len > 0) {
            module->manu_req_count = 0;
            module->state = AUTO_REQ;
            return;
        }
    }

    module_shutdown(module);
}


bool module_is_loaded(const module_t *module)
{
    return module->state == AUTO_REQ || module->state == MANU_REQ;
}

bool module_is_initializing(const module_t *module)
{
    return module->state == INITIALIZING;
}

bool module_is_shutting_down(const module_t *module)
{
    return module->state == SHUTTING;
}

const char *module_get_name(const module_t *module)
{
    return module->name.s;
}

static void module_hard_shutdown(void)
{
    /* Shutdown manually required modules that were not released. */
    qm_for_each_pos(module, pos, &_G.modules) {
        module_t *module = _G.modules.values[pos];

        if (module->state == MANU_REQ) {
            bool warned = false;

            while (module->manu_req_count && !(module->state & FAIL_SHUT)) {
                if (!warned) {
                    logger_trace(&_G.logger, 1,
                                 "%*pM was not released, forcing release",
                                 LSTR_FMT_ARG(module->name));
                    warned = true;
                }
                module_release(module);
            }
        }
    }

    /* All modules should be shutdown now. */
    qm_for_each_pos(module, pos, &_G.modules) {
        module_t *module = _G.modules.values[pos];

        assert (module->state == REGISTERED || module->state & FAIL_SHUT);
    }
}

extern bool syslog_is_critical;

static void module_dep_qh_wipe(qh_t(ptr) *qh)
{
    qh_wipe(ptr, qh);
}

__attribute__((destructor))
static void _module_shutdown(void)
{
    if (_G.is_shutdown) {
        return;
    }

    if (!syslog_is_critical) {
        module_hard_shutdown();
    }
    qm_deep_wipe(methods_impl, &_G.methods, IGNORE, module_method_delete);
    qm_deep_wipe(module, &_G.modules, IGNORE, module_delete);
    qm_wipe(module_arg, &_G.modules_arg);
    qm_deep_wipe(module_dep, &_G.module_dep_resolve, IGNORE,
                 module_dep_qh_wipe);
    logger_wipe(&_G.logger);
    _G.is_shutdown = true;
}

void module_destroy_all(void)
{
    _module_shutdown();
}

/* }}} */
/* {{{ Methods */

void module_implement_method(module_t *module, const module_method_t *method,
                             void *cb)
{
    int pos;

    pos = qm_reserve(methods_impl, &_G.methods, method, 0);
    if (!(pos & QHASH_COLLISION)) {
        module_method_impl_t *m;

        m = module_method_new();

        m->params = method;
        _G.methods.values[pos] = m;
    }
    qm_add(methods, &module->methods, method, cb);
}

void module_run_method(const module_method_t *method, data_t arg)
{
    module_method_impl_t *m;

    m = qm_get_def(methods_impl, &_G.methods, method, NULL);
    if (!m) {
        /* method not implemented */
        return;
    }

    switch (method->type) {
      case METHOD_VOID:
        qv_for_each_entry(methods_cb, cb, &m->callbacks) {
            ((void (*)(void))cb)();
        }
        break;

      case METHOD_INT:
        qv_for_each_entry(methods_cb, cb, &m->callbacks) {
            ((void (*)(int))cb)(arg.u32);
        }
        break;

      case METHOD_PTR:
      case METHOD_GENERIC:
        qv_for_each_entry(methods_cb, cb, &m->callbacks) {
            ((void (*)(data_t))cb)(arg);
        }
        break;
    }
}

static void
module_add_method(module_t *module, module_method_impl_t *method,
                  qh_t(ptr) *already_run)
{
    void *cb = qm_get_def(methods, &module->methods, method->params, NULL);

    if (cb) {
        qv_append(methods_cb, &method->callbacks, cb);
    }
}

static void rec_module_run_method(module_t *module,
                                  module_method_impl_t *method,
                                  qh_t(ptr) *already_run)
{
    int pos = qh_put(ptr, already_run, module, 0);

    assert (module_is_loaded(module));
    if (pos & QHASH_COLLISION) {
        return;
    }

    if (method->params->order == MODULE_DEPS_AFTER) {
        qv_for_each_entry(module, dep, &module->required_by) {
            if (module_is_loaded(dep)) {
                rec_module_run_method(dep, method, already_run);
            }
        }
        module_add_method(module, method, already_run);
    }

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        rec_module_run_method(qm_get(module, &_G.modules, &dep), method,
                              already_run);
    }

    if (method->params->order == MODULE_DEPS_BEFORE) {
        module_add_method(module, method, already_run);
    }
}

static void module_build_method_cb(module_method_impl_t *method)
{
    qh_t(ptr) already_run;

    qv_clear(methods_cb, &method->callbacks);

    qh_init(ptr, &already_run);
    /* First pass: run the method from all manual required modules. */
    qm_for_each_pos(module, position, &_G.modules) {
        module_t *module = _G.modules.values[position];

        if (module->state == MANU_REQ && module->required_by.len == 0) {
            rec_module_run_method(module, method, &already_run);
        }
    }

    if (!_G.in_initialization) {
        qh_wipe(ptr, &already_run);
        return;
    }

    /* Second pass: run the method on auto required modules that might not
     * have been run on the first pass.
     * This can happen when you have a module that run its method during its
     * initialization since the state of the module is set only when it is
     * completly initialized.
     */
    qm_for_each_pos(module, position, &_G.modules) {
        module_t *module = _G.modules.values[position];

        if (module->state == AUTO_REQ) {
            rec_module_run_method(module, method, &already_run);
        }
    }

    qh_wipe(ptr, &already_run);
}

static void module_build_method_all_cb(void)
{
    qm_for_each_pos(methods_impl, pos, &_G.methods) {
        module_method_impl_t *m = _G.methods.values[pos];

        module_build_method_cb(m);
    }
}

MODULE_METHOD(INT, DEPS_BEFORE, on_term);

void module_on_term(int signo)
{
    module_run_method(&on_term_method, (el_data_t){ .u32 = signo });
}

MODULE_METHOD(VOID, DEPS_AFTER, at_fork_prepare);
MODULE_METHOD(VOID, DEPS_BEFORE, at_fork_on_parent);
MODULE_METHOD(VOID, DEPS_BEFORE, at_fork_on_child);
MODULE_METHOD(VOID, DEPS_BEFORE, consume_child_events);

static void module_at_fork_prepare(void)
{
    MODULE_METHOD_RUN_VOID(at_fork_prepare);
}

static void module_at_fork_on_parent(void)
{
    MODULE_METHOD_RUN_VOID(at_fork_on_parent);
}

static void module_at_fork_on_child(void)
{
    MODULE_METHOD_RUN_VOID(at_fork_on_child);
}

#ifndef SHARED
__attribute__((constructor))
#endif
void module_register_at_fork(void)
{
    static bool at_fork_registered = false;

    if (!at_fork_registered) {
        void *data = NULL;

        /* XXX: ensures internal ressources of the libc used to implement
         * posix_memalign() are ready here. It looks like the glibc of some
         * CentOs use an atfork() handler that get registered the first time
         * you perform an aligned allocation.
         */
        IGNORE(posix_memalign(&data, 64, 1024));
        free(data);

        pthread_atfork(module_at_fork_prepare,
                       module_at_fork_on_parent,
                       module_at_fork_on_child);
        at_fork_registered = true;
    }
}

/* }}} */
/* {{{ Dependency collision */

/** Adds to qh all the modules that are dependent of the module m.
 */
static void add_dependencies_to_qh(module_t *m, qh_t(lstr) *qh)
{
    qv_for_each_entry(lstr, e, &m->dependent_of) {
        if (qh_add(lstr, qh, &e) >= 0) {
            module_t *mod;
            int pos = qm_find(module, &_G.modules, &e);

            if (!expect(pos >= 0)) {
                logger_error(&_G.logger, "unknown module `%*pM`",
                             LSTR_FMT_ARG(e));
                continue;
            }
            mod = _G.modules.values[pos];
            add_dependencies_to_qh(mod, qh);
        }
    }
}

int module_check_no_dependencies(module_t *tab[], int len,
                                 lstr_t *collision)
{
    t_scope;
    qh_t(lstr) dependencies;

    t_qh_init(lstr, &dependencies, len);
    for (int pos = 0; pos < len; pos++) {
        add_dependencies_to_qh(tab[pos], &dependencies);
    }
    for (int pos = 0; pos < len; pos++) {
        if (qh_find(lstr, &dependencies, &tab[pos]->name) >= 0) {
            *collision = tab[pos]->name;
            return -1;
        }
    }
    return 0;
}

/* }}} */

/* {{{ Debug */

void module_debug_dump_hierarchy(sb_t *modules, sb_t *dependencies)
{
    sb_sets(modules, "nodes;loaded\n");
    sb_sets(dependencies, "nodes;dest\n");
    qm_for_each_pos(module, pos, &_G.modules) {
        module_t *module = _G.modules.values[pos];

        sb_addf(modules, "%*pM;%d\n", LSTR_FMT_ARG(module->name),
                module_is_loaded(module) ? 1 : 0);
        qv_for_each_entry(lstr, dep_name, &module->dependent_of) {
            sb_addf(dependencies, "%*pM;%*pM\n", LSTR_FMT_ARG(module->name),
                    LSTR_FMT_ARG(dep_name));
        }
    }
}

/* }}} */
