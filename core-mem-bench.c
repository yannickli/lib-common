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

#include "core-mem-bench.h"
#include "thr.h"

static void mem_bench_print_func_csv(mem_bench_func_t *spf, FILE *file)
{
    fprintf(file, "%u,%u,",
            spf->nb_calls, spf->nb_slow_path);
    fprintf(file, "%u,%lu,%lu,%lu,",
            spf->timer_stat.nb,
            spf->timer_stat.hard_min,
            spf->timer_stat.hard_max,
            spf->timer_stat.hard_tot);
}

void mem_bench_print_csv(mem_bench_t *sp, const char *context, FILE *file)
{
    assert (file);
    fprintf(file, "%s,%ld,%p,",
            context, (long)thr_id(), sp);
    mem_bench_print_func_csv(&sp->alloc, file);
    mem_bench_print_func_csv(&sp->realloc, file);
    mem_bench_print_func_csv(&sp->free, file);
    fprintf(file, "%lu,%lu,%u,%u,%u,%u,%u,%u\n",
            sp->total_allocated, sp->total_requested,
            sp->max_allocated, sp->max_unused, sp->max_used,
            sp->malloc_calls, sp->current_used, sp->current_allocated);
}

void mem_bench_update_max(mem_bench_t *sp)
{
    sp->max_used      = MAX(sp->current_used, sp->max_used);
    sp->max_allocated = MAX(sp->current_allocated, sp->max_allocated);
    sp->max_unused    = MAX(sp->current_allocated - sp->current_used,
                            sp->max_unused);
}

static void mem_bench_print_func_human(mem_bench_func_t *spf)
{
    printf("    requests               : %10u \n", spf->nb_calls);
    printf("    slow path calls        : %10u \n", spf->nb_slow_path);
    printf("    %s", proctimerstat_report(&spf->timer_stat, "%h"));
}

void mem_bench_print_human(mem_bench_t *sp, int flags)
{
    if (flags & MEM_BENCH_PRINT_CURRENT) {
        printf("\x1b[32m");
    } else {
        printf("\x1b[31m");
    }
    printf("Stack allocator @%p stats  :\x1b[0m\n", sp);
    printf("  alloc                    :\n");
    mem_bench_print_func_human(&sp->alloc);
    printf("  realloc                  :\n");
    mem_bench_print_func_human(&sp->realloc);
    printf("  free                     :\n");
    mem_bench_print_func_human(&sp->free);
    printf("  average request size     : %10lu bytes \n",
           sp->total_requested / MAX(1, sp->alloc.nb_calls));
    printf("  average block size       : %10lu bytes\n",
           sp->total_allocated / MAX(1, sp->malloc_calls));
    printf("  total memory allocated   : %10lu K \n",
           sp->total_allocated / 1024);
    printf("  total memory requested   : %10lu K \n",
           sp->total_requested / 1024);
    printf("  max used memory          : %10d K \n",
           sp->max_used / 1024);
    printf("  max unused memory        : %10u K \n",
           sp->max_unused / 1024);
    printf("  max memory allocated     : %10u K \n",
           sp->max_allocated / 1024);
    printf("  malloc calls             : %10u \n",
           sp->malloc_calls);
    if (flags & MEM_BENCH_PRINT_CURRENT) {
        printf("  current used memory      : %10u K \n",
               sp->current_used / 1024);
        printf("  current allocated memory : %10u K \n",
               sp->current_allocated / 1024);
    }
}
