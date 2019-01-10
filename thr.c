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

#include "thr.h"

struct thr_hooks thr_hooks_g = {
    .init_cbs = DLIST_INIT(thr_hooks_g.init_cbs),
    .exit_cbs = DLIST_INIT(thr_hooks_g.exit_cbs),
};

static void thr_main_atexit(void)
{
    dlist_for_each(it, &thr_hooks_g.exit_cbs) {
        (container_of(it, struct thr_ctor, link)->cb)();
    }
}

__attribute__((constructor))
static void thr_run_dtors_at_exit(void)
{
    atexit(thr_main_atexit);
}
