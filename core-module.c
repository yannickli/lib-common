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

typedef enum module_state_t {
    REGISTERED = 0,
    AUTO_REQ   = 1 << 0, /* Initialized automatically */
    MANU_REQ   = 1 << 1, /* Initialize manually */
    FAIL_SHUT  = 1 << 2, /* Fail to shutdown */
} module_state_t;

qvector_t(module, module_t *);
qm_kptr_ckey_t(methods, void, void *, qhash_hash_ptr, qhash_ptr_equal);

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
    qm_init(methods, &module->methods, false);
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

/* }}} */
/* {{{ Module Registry */

static struct _module_g {
    logger_t logger;
    qm_t(module)     modules;
    qm_t(module_arg) modules_arg;
} module_g = {
#define _G module_g
    .logger = LOGGER_INIT(NULL, "module", LOG_INHERITS),
    .modules     = QM_INIT(module, _G.modules, false),
    .modules_arg = QM_INIT(module_arg, _G.modules_arg, false),
};


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
    int pos = qm_reserve(module, &_G.modules, &name, 0);
    int arg_pos;

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
        qv_append(lstr, &new_module->dependent_of, LSTR_STR_V(dependencies[i]));
    }

    _G.modules.keys[pos] = &new_module->name;
    _G.modules.values[pos] = new_module;

    return new_module;
}

void module_add_dep(module_t *module, lstr_t dep, module_t **dep_ptr)
{
    /* XXX dep_ptr is used only to force an explicit dependency between
     * modules. This guarantees that if module is present in the binary, the
     * dependency will be present too.
     */
    assert (module->state == REGISTERED);
    qv_append(lstr, &module->dependent_of, dep);
}

void module_require(module_t *module, module_t *required_by)
{
    if (module->state == AUTO_REQ || module->state == MANU_REQ) {
        set_require_type(module, required_by);
        return;
    }

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        module_require(qm_get(module, &_G.modules, &dep), module);
    }

    if ((*module->constructor)(module->constructor_argument) >= 0) {
        set_require_type(module, required_by);
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
    int shut_self, shut_dependent;

    shut_self = shut_dependent = 1;

    if ((shut_self = (*module->destructor)()) >= 0) {
        module->state = REGISTERED;
        module->manu_req_count = 0;
    } else {
        logger_warning(&_G.logger, "unable to shutdown   %*pM",
                       LSTR_FMT_ARG(module->name));
        module->state = FAIL_SHUT;
    }

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        int shut;

        shut = notify_shutdown(qm_get(module, &_G.modules, &dep), module);
        if (shut < 0) {
            shut_dependent = shut;
        }
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
        assert (false && "unauthorize release");
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


/**  - __attribute__((destructor))
 *      static void _module_shutdown(void)
 *   -  static int module_hard_shutdown(void)
 *
 *  When exiting, shutdown the modules that are still open
 *  The order of the loop is important
 */
static int module_hard_shutdown(void)
{
    int error = 0;

    /* Shuting down modules that was not released */
    qm_for_each_pos(module, position, &_G.modules){
        module_t *module = _G.modules.values[position];

        if (module->state == MANU_REQ) {
            while (module->manu_req_count
            && (module->state & FAIL_SHUT) != FAIL_SHUT)
            {
                error--;
                logger_trace(&_G.logger, 1, "%*pM was not released "
                             "(forcing release)", LSTR_FMT_ARG(module->name));
                module_release(module);
            }
        }
    }


    /* Shuting down automatic modules that might be still open */
    qm_for_each_pos(module, position, &_G.modules) {
        module_t *module = _G.modules.values[position];

        assert (module->state != AUTO_REQ && module->state != MANU_REQ);
    }
    return error;
}

extern bool syslog_is_critical;

__attribute__((destructor))
static void _module_shutdown(void)
{
    if (!syslog_is_critical) {
        module_hard_shutdown();
    }
    qm_deep_wipe(module, &_G.modules, IGNORE, module_delete);
    qm_wipe(module_arg, &_G.modules_arg);
    logger_wipe(&_G.logger);
}

/* }}} */
/* {{{ Methods */

void module_implement_method(module_t *module, const module_method_t *method,
                             void *cb)
{
    qm_add(methods, &module->methods, method, cb);
}

static void do_run_method(module_t *module, const module_method_t *method,
                          data_t arg, qh_t(ptr) *already_run)
{
    void *cb = qm_get_def(methods, &module->methods, method, NULL);

    if (!cb) {
        return;
    }

    switch (method->type) {
      case METHOD_VOID:
        ((void (*)(void))cb)();
        break;

      case METHOD_INT:
        ((void (*)(int))cb)(arg.u32);
        break;

      case METHOD_PTR:
      case METHOD_GENERIC:
        ((void (*)(data_t))cb)(arg);
        break;
    }
}

static void rec_module_run_method(module_t *module,
                                  const module_method_t *method, data_t arg,
                                  qh_t(ptr) *already_run)
{
    int pos = qh_put(ptr, already_run, module, 0);

    assert (module->state != REGISTERED);
    if (pos & QHASH_COLLISION) {
        return;
    }

    if (method->order == MODULE_DEPS_AFTER) {
        qv_for_each_entry(module, dep, &module->required_by) {
            if (dep->state != REGISTERED) {
                rec_module_run_method(dep, method, arg, already_run);
            }
        }
        do_run_method(module, method, arg, already_run);
    }

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        rec_module_run_method(qm_get(module, &_G.modules, &dep), method,
                              arg, already_run);
    }

    if (method->order == MODULE_DEPS_BEFORE) {
        do_run_method(module, method, arg, already_run);
    }
}

void module_run_method(const module_method_t *method, data_t arg)
{
    qh_t(ptr) already_run;

    qh_init(ptr, &already_run, false);
    qm_for_each_pos(module, position, &_G.modules) {
        module_t *module = _G.modules.values[position];

        if (module->state == MANU_REQ && module->required_by.len == 0) {
            rec_module_run_method(module, method, arg, &already_run);
        }
    }

    qh_wipe(ptr, &already_run);
}

MODULE_METHOD(INT, DEPS_BEFORE, on_term);

void module_on_term(int signo)
{
    module_run_method(&on_term_method, (el_data_t){ .u32 = signo });
}

MODULE_METHOD(VOID, DEPS_AFTER, at_fork_prepare);
MODULE_METHOD(VOID, DEPS_BEFORE, at_fork_on_parent);
MODULE_METHOD(VOID, DEPS_BEFORE, at_fork_on_child);

#ifndef SHARED
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

static __attribute__((constructor))
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
#endif

/* }}} */
/*{{{ Dependency collision */

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

    t_qh_init(lstr, &dependencies, false, len);
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

/*}}} */
