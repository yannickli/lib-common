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

#include "log.h"
#include "container.h"

/*{{{ Type definition */

typedef enum module_state_t {
    REGISTERED = 0,
    AUTO_REQ   = 1 << 0, /* Initialized automatically */
    MANU_REQ   = 1 << 1, /* Initialize manually */
    FAIL_REQ   = 1 << 2, /* Fail to initialize */
    FAIL_SHUT  = 1 << 3, /* Fail to shutdown */
    FAIL_REQ_AND_SHUT = FAIL_REQ | FAIL_SHUT
} module_state_t;

typedef struct module_t {
    lstr_t name;
    module_state_t state;
    int manu_req_count;

    /* Vector of module name */
    qv_t(lstr) dependent_of;
    qv_t(lstr) required_by;


    int (*constructor)(void *);
    int (*destructor)(void);
    void (*on_term)(int);
    void *constructor_argument;

} module_t;


GENERIC_NEW_INIT(module_t, module);
static inline module_t *module_wipe(module_t *module)
{
    lstr_wipe(&(module->name));
    qv_deep_wipe(lstr, &module->dependent_of, lstr_wipe);
    qv_deep_wipe(lstr, &module->required_by, lstr_wipe);
    return module;
}
GENERIC_DELETE(module_t, module);

qm_kptr_t(module, lstr_t, module_t *, qhash_lstr_hash, qhash_lstr_equal);

/*}}}*/

static struct _module_g {
    logger_t logger;
    qm_t(module) modules;
} module_g = {
#define _G module_g
    .logger = LOGGER_INIT(NULL, "module", LOG_INHERITS),
    .modules = QM_INIT(module, _G.modules, false),
};


static module_t *find_module(lstr_t name)
{
    int pos;
    pos = qm_find(module, &_G.modules, &name);
    assert (pos >= 0);
    return _G.modules.values[pos];
}


static void
set_require_type(lstr_t name, lstr_t required_by, module_t *module)
{
    if (lstr_equal2(required_by, LSTR_NULL_V)) {
        module->state = MANU_REQ;
        module->manu_req_count++;
    } else {
        qv_append(lstr, &module->required_by, lstr_dupc(required_by));
        if (module->state != MANU_REQ)
            module->state = AUTO_REQ;
    }
}


static void rec_module_on_term(lstr_t name, int signo)
{
    module_t *module = find_module(name);

    if (module->on_term)
        (*module->on_term)(signo);

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        rec_module_on_term(dep, signo);
    }
}

void module_on_term(int signo)
{
    qm_for_each_pos(module, position, &_G.modules){
        module_t *module = _G.modules.values[position];

        if (module->state == MANU_REQ && module->required_by.len == 0) {
            rec_module_on_term(module->name, signo);
        }
    }
}


int module_register(lstr_t name, int (*constructor)(void *),
                    void (*on_term)(int signo),
                    int (*destructor)(void), ...)
{
    module_t *new_module;
    va_list vl;
    const char *dependence;
    int pos = __qm_reserve(module, &_G.modules, &name, 0);

    if (pos & QHASH_COLLISION) {
        logger_warning(&_G.logger,
                       "%*pM has already been register", LSTR_FMT_ARG(name));
        return F_ALREADY_REGISTERED;
    }

    new_module = module_new();
    new_module->state = REGISTERED;
    new_module->name = lstr_dupc(name);
    new_module->manu_req_count = 0;
    new_module->constructor_argument = NULL;
    new_module->constructor = constructor;
    new_module->on_term = on_term;
    new_module->destructor = destructor;

    va_start(vl, destructor);
    while ((dependence = va_arg(vl, const char *)) != NULL) {
        qv_append(lstr, &new_module->dependent_of, LSTR_STR_V(dependence));
    }
    va_end(vl);

    _G.modules.keys[pos] = &new_module->name;
    _G.modules.values[pos] = new_module;

    return F_REGISTER;
}


int module_require(lstr_t name, lstr_t required_by)
{
    module_t *module = find_module(name);

    if (module->state == AUTO_REQ || module->state == MANU_REQ) {
        set_require_type(name, required_by, module);
        return F_INITIALIZE;
    }

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        int res = module_require(dep, name);

        if (res == F_NOT_INITIALIZE)
            module->state = FAIL_REQ;
        RETHROW(res);
    }

    if ((*module->constructor)(module->constructor_argument) >= 0) {
        set_require_type(name, required_by, module);
        return F_INITIALIZE;
    }

    logger_warning(&_G.logger, "Unable to initialize %*pM",
                   LSTR_FMT_ARG(name));
    module->state = FAIL_REQ;
    return F_NOT_INITIALIZE;
}



void module_provide(lstr_t name, void *argument)
{
    module_t *module = find_module(name);

    module->constructor_argument = argument;
}

static int notify_shutdown(lstr_t name, lstr_t dependent_of)
{
    module_t *module = find_module(name);

    qv_for_each_pos(lstr, position, &module->required_by) {
        if (lstr_equal(&module->required_by.tab[position], &dependent_of)) {
            qv_remove(lstr, &module->required_by, position);
            break;
        }
    }
    if (module->required_by.len == 0 && module->state != MANU_REQ)
        return module_shutdown(module->name);

    return F_NOTIFIED;
}


int module_shutdown(lstr_t name)
{
    int shut_self, shut_dependent;
    module_t *module = find_module(name);

    shut_self = shut_dependent = 1;

    if (module->state == FAIL_REQ) {
        shut_self = 1;
    } else
    if ((shut_self = (*module->destructor)()) >= 0) {
        module->state = REGISTERED;
        module->manu_req_count = 0;
    } else {
        logger_warning(&_G.logger, "Unable to shutdown   %*pM",
                       LSTR_FMT_ARG(name));
        module->state = FAIL_SHUT | (module->state & FAIL_REQ);
    }

    qv_for_each_entry(lstr, dep, &module->dependent_of) {
        int shut;
        shut = notify_shutdown(dep, name);
        if (shut < 0)
            shut_dependent = shut;
    }

    if (shut_dependent < 0 || shut_self < 0)
        return F_NOT_SHUTDOWN;

    return F_SHUTDOWN;
}



int module_release(lstr_t name)
{
    module_t *module = find_module(name);

    if (module->manu_req_count == 0) {
        /* You are trying to either release:
         *   - manually a module that have been spawn automatically (AUTO_REQ)
         *   - a module that is not loaded (F_REGISTER)
         */
        assert (false && "Unauthorize release");
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

    return module_shutdown(name);
}


bool module_is_loaded(lstr_t name)
{
    module_t * module = find_module(name);

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
                logger_warning(&_G.logger, "%*pM was not released",
                               LSTR_FMT_ARG(module->name));
                module_release(module->name);
            }
        }
    }


    /* Shuting down automatic modules that might be still open */
    qm_for_each_pos(module, position, &_G.modules){
        module_t *module = _G.modules.values[position];

        if (module->state == AUTO_REQ) {
            error--;
            logger_warning(&_G.logger, "%*pM was not released",
                           LSTR_FMT_ARG(module->name));
            module_shutdown(module->name);
        }
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
