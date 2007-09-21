/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <boehm/include/gc.h>
#include "mem.h"
#include "err_report.h"

void mem_initialize(void)
{
    GC_init();
    GC_set_warn_proc(NULL);
    GC_quiet = 1;
    GC_disable();
}

void mem_check(void)
{
    static size_t last_used = 0;
    size_t brksz, freesz, usedsz, free_after;

    brksz  = GC_get_heap_size();
    freesz = GC_get_free_bytes();
    usedsz = brksz - freesz;

    if (!last_used) {
        last_used = brksz - freesz;
        return;
    }

    /* don't act if the daemon uses less than 64Mb of ram */
    if (usedsz < ((size_t)64 << 20))
        return;

    /* act on more than 10% of memory usage growth only */
    if (usedsz < ((size_t)2 << 30) && usedsz < last_used * 9 / 8)
        return;

    GC_enable();
    GC_gcollect();
    free_after = GC_get_free_bytes();
    if (free_after > freesz) {
        size_t freed = free_after - freesz;
        if (freed > 1 << 20) {
            e_error("GC: %zdMo freed by the gc", freed >> 20);
        } else
        if (freed > 1 << 10) {
            e_error("GC: %zdKo freed by the gc", freed >> 10);
        } else {
            e_error("GC: %zdo freed by the gc", freed);
        }
    }
    last_used = brksz - free_after;
    GC_disable();
}
