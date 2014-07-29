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
    FILE      *file;
    uint32_t   out_period;
    uint32_t   out_counter;

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
} mem_bench_t;

void mem_bench_init(mem_bench_t *sp, lstr_t type, uint32_t period);
void mem_bench_wipe(mem_bench_t *sp);

void mem_bench_update(mem_bench_t *sp);
void mem_bench_print_csv(mem_bench_t *sp);
void mem_bench_print_human(mem_bench_t *sp, int flags);

#define MEM_BENCH_PRINT_CURRENT  1

#endif /* IS_LIB_COMMON_CORE_MEM_BENCH_H */
