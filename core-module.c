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

#include "log.h"
#include "container.h"

/* {{{ Type definition */

typedef enum module_state_t {
    REGISTERED = 0,
    AUTO_REQ   = 1 << 0, /* Initialized automatically */
    MANU_REQ   = 1 << 1, /* Initialize manually */
    FAIL_SHUT  = 1 << 2, /* Fail to shutdown */
} module_state_t;

qvector_t(module, module_t *);
struct module_t {
    lstr_t name;
    module_state_t state;
    int manu_req_count;

    /* Vector of module name */
    qv_t(lstr)   dependent_of;
    qv_t(module) required_by;


    int (*constructor)(void *);
    int (*destructor)(void);
    void (*on_term)(int);
    void *constructor_argument;
};

GENERIC_NEW_INIT(module_t, module);
static inline module_t *module_wipe(module_t *module)
{
    lstr_wipe(&(module->name));
    qv_deep_wipe(lstr, &module->dependent_of, lstr_wipe);
    qv_wipe(module, &module->required_by);
    return module;
}
GENERIC_DELETE(module_t, module);

qm_kptr_t(module, lstr_t, module_t *, qhash_lstr_hash, qhash_lstr_equal);

/* }}} */
/* {{{ Module Registry */

static struct _module_g {
    logger_t logger;
    qm_t(module) modules;
} module_g = {
#define _G module_g
    .logger = LOGGER_INIT(NULL, "module", LOG_INHERITS),
    .modules = QM_INIT(module, _G.modules, false),
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


static void rec_module_on_term(module_t *module, int signo)
{
    if (module->on_term)
        (*module->on_term)(signo);

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        rec_module_on_term(qm_get(module, &_G.modules, &dep), signo);
    }
}

void module_on_term(int signo)
{
    qm_for_each_pos(module, position, &_G.modules){
        module_t *module = _G.modules.values[position];

        if (module->state == MANU_REQ && module->required_by.len == 0) {
            rec_module_on_term(module, signo);
        }
    }
}


module_t *module_register(lstr_t name, int (*constructor)(void *),
                          void (*on_term)(int signo), int (*destructor)(void),
                          const char *dependencies[], int nb_dependencies)
{
    module_t *new_module;
    int pos = qm_reserve(module, &_G.modules, &name, 0);

    if (pos & QHASH_COLLISION) {
        logger_warning(&_G.logger,
                       "%*pM has already been register", LSTR_FMT_ARG(name));
        return NULL;
    }

    new_module = module_new();
    new_module->state = REGISTERED;
    new_module->name = lstr_dupc(name);
    new_module->manu_req_count = 0;
    new_module->constructor_argument = NULL;
    new_module->constructor = constructor;
    new_module->on_term = on_term;
    new_module->destructor = destructor;

    for (int i = 0; i < nb_dependencies; i++) {
        qv_append(lstr, &new_module->dependent_of, LSTR_STR_V(dependencies[i]));
    }

    _G.modules.keys[pos] = &new_module->name;
    _G.modules.values[pos] = new_module;

    return new_module;
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



void module_provide(module_t *module, void *argument)
{
    module->constructor_argument = argument;
}

static int notify_shutdown(module_t *module, module_t *dependence)
{
    qv_for_each_pos(module, pos, &module->required_by) {
        if (module->required_by.tab[pos] == dependence) {
            qv_remove(module, &module->required_by, pos);
            break;
        }
    }
    if (module->required_by.len == 0 && module->state != MANU_REQ)
        return module_shutdown(module);

    return F_NOTIFIED;
}


int module_shutdown(module_t *module)
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
        if (shut < 0)
            shut_dependent = shut;
    }

    if (shut_dependent < 0 || shut_self < 0)
        return F_NOT_SHUTDOWN;

    return F_SHUTDOWN;
}



int module_release(module_t *module)
{
    if (module->manu_req_count == 0) {
        /* You are trying to either release:
         *   - manually a module that have been spawn automatically (AUTO_REQ)
         *   - a module that is not loaded (F_REGISTER)
         */
        assert (false && "unauthorize release");
        return F_UNAUTHORIZE_RELEASE;
    }

    if (module->state == MANU_REQ && module->manu_req_count > 1) {
        module->manu_req_count--;
        return F_RELEASED;
    }

    if (module->state == MANU_REQ && module->manu_req_count == 1) {
        if (module->required_by.len > 0) {
            module->manu_req_count = 0;
            module->state = AUTO_REQ;
            return F_RELEASED;
        }
    }

    return module_shutdown(module);
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

__attribute__((destructor))
static void _module_shutdown(void)
{
    module_hard_shutdown();
    qm_deep_wipe(module, &_G.modules, IGNORE, module_delete);
    logger_wipe(&_G.logger);
}

/* }}} */
