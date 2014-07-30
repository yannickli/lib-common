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

#ifndef IS_LIB_COMMON_CORE_MEM_BENCH_H
#define IS_LIB_COMMON_CORE_MEM_BENCH_H

#include "core.h"
#include "datetime.h"

/* for timing individual function */
typedef struct mem_bench_func_t {
    uint32_t nb_calls;
    uint32_t nb_slow_path;

    proctimerstat_t timer_stat;
} mem_bench_func_t;

typedef struct mem_bench_t {
    /* CSV dumping */
    FILE      *file;
    uint32_t   out_period;
    uint32_t   out_counter;

    /* profile data */
    mem_bench_func_t alloc;
    mem_bench_func_t realloc;
    mem_bench_func_t free;

    uint64_t total_allocated;
    uint64_t total_requested;

    uint32_t max_allocated;
    uint32_t max_used;
    uint32_t max_unused;

    uint32_t malloc_calls;
    uint32_t current_used;
    uint32_t current_allocated;

    /* allocator type */
    lstr_t   allocator_name;
} mem_bench_t;

/** Initialize mem_bench object.
 *
 * The mem_bench object is initialized to dump into a file
 * each \p period iterations. The filename is
 * "./mem.[\p name].[pid].[address of \p sp]".
 *
 * \param[in] type   Name of mem_bench object.
 * \param[in] period Logging period. 0 means no logging.
 */
mem_bench_t *mem_bench_init(mem_bench_t *sp, lstr_t type, uint32_t period);
void mem_bench_wipe(mem_bench_t *sp);

__unused__
static inline mem_bench_t *mem_bench_new(lstr_t type, uint32_t period)
{
    return mem_bench_init(p_new_raw(mem_bench_t, 1), type, period);
}
GENERIC_DELETE(mem_bench_t, mem_bench)

/** Update state of the mem-bench object.
 *
 * This function updates the max_* fields of \p sp
 * and is responsible for periodic dumping to file.
 */
void mem_bench_update(mem_bench_t *sp);

/** Manually dump fields. */
void mem_bench_print_csv(mem_bench_t *sp);

/** Print stats in human-readable form.
 *
 * \param[in] flags Flag controlling printed informations.
 */
void mem_bench_print_human(mem_bench_t *sp, int flags);

/** Flag for print_human : print current allocation status. */
#define MEM_BENCH_PRINT_CURRENT  1

#endif /* IS_LIB_COMMON_CORE_MEM_BENCH_H */
