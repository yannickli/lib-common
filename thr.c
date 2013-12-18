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

#include "thr.h"

struct thr_hooks thr_hooks_g = {
    .init_cbs = DLIST_INIT(thr_hooks_g.init_cbs),
    .exit_cbs = DLIST_INIT(thr_hooks_g.exit_cbs),
};

static int thr_hooks_initialize(void *arg)
{
    return 0;
}

static int thr_hooks_shutdown(void)
{
    dlist_for_each(it, &thr_hooks_g.exit_cbs) {
        (container_of(it, struct thr_ctor, link)->cb)();
    }
    return 0;
}

__attribute__((constructor))
static void thr_run_dtors_at_exit(void)
{
    static module_t *thr_hooks_module;

    thr_hooks_module = MODULE_REGISTER(thr_hooks, NULL);
    MODULE_REQUIRE(thr_hooks);
}
