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

#include "core.h"
#include "datetime.h"
#include "parseopt.h"

/* Small utility to benchmark allocators stack and fifo allocators
 * run ztst-mem-bench -f to test the fifo allocator
 *                    -s to test the stack allocator
 */

static struct {
    int help;
    int verbose;
    int test_stack;
    int test_fifo;
    int worst_case;
    int num_allocs;
    int max_allocated;
    int max_alloc_size;
    int max_depth;
    int num_tries;
} settings = {
    .num_allocs = 1 << 20,
    .max_allocated = 10000,
    .max_alloc_size = 512,
    .max_depth = 1500,
    .num_tries = 100,
};

static popt_t popts[] = {
    OPT_FLAG('h', "help", &settings.help, "show this help"),
    OPT_FLAG('s', "stack", &settings.test_stack, "test stack allocator"),
    OPT_FLAG('f', "fifo", &settings.test_fifo, "test fifo allocator"),
    OPT_FLAG('w', "worst-case", &settings.worst_case,
             "worst case test (fifo)"),
    OPT_INT('n', "allocs", &settings.num_allocs,
            "number of allocations made (default: 1 << 25)"),
    OPT_INT('m', "max", &settings.max_allocated, "max number of"
            " simultaneously allocated blocks (fifo only, default 10000)"),
    OPT_INT('z', "size", &settings.max_alloc_size,
            "max size of an allocation"),
    OPT_INT('d', "depth", &settings.max_depth, "max stack height (stack only"
            ", default 1500)"),
    OPT_INT('r', "tries", &settings.num_tries, "number of retries (stack only"
            ", default 100"),
    OPT_END(),
};


/** Fifo allocator benchmarking
 *
 * First check is under real fifo behaviour: every block allocated is freed
 * immediately
 *
 * Second check is randomized: blocks are deallocated at a random time, and at
 * most MAX_ALLOCATED blocks are simultaneously allocated
 */
static int benchmark_fifo(void)
{
    mem_pool_t *mp_fifo = mem_fifo_pool_new(0);
    byte **table = p_new(byte *, settings.max_allocated);

    /* Real fifo behavior, one at a time */
    for (int i = 0; i < settings.num_allocs / 3; i++) {
        byte *a = mp_new_raw(mp_fifo, byte,
                             rand() % settings.max_alloc_size);
        mp_ifree(mp_fifo, a);
    }

    /* Real fifo behaviour, settings.max_allocated at a time */
    for (int i = 0; i < settings.num_allocs / 3; i++) {
        int chosen = i % settings.max_allocated;

        if (table[chosen]) {
            mp_ifree(mp_fifo, table[chosen]);
        }
        table[chosen] = mp_new_raw(mp_fifo, byte,
                                   rand() % settings.max_alloc_size);
    }

    /* Almost fifo */
    for (int i = 0; i < settings.num_allocs / 3; i++) {
        int chosen = rand() % settings.max_allocated;

        if (table[chosen]) {
            mp_ifree(mp_fifo, table[chosen]);
        }
        table[chosen] = mp_new_raw(mp_fifo, byte,
                                   rand() % settings.max_alloc_size);
    }

    /* Clean leftovers */
    for (int i = 0; i < settings.max_allocated; i++) {
        if (table[i]) {
            mp_ifree(mp_fifo, table[i]);
        }
    }

    p_delete(&table);
    mem_fifo_pool_delete(&mp_fifo);
    return 0;
}

#if 1
/** This one tries to trigger an allocation each time
 */
static int benchmark_fifo_worst_case(void)
{
    mem_pool_t *mp = mem_fifo_pool_new(32 * 4096);

    for (int i = 0; i < settings.num_allocs; i++) {
        void *a = mp_new_raw(mp, byte, 32 * 4096 + i);

        mp_ifree(mp, a);
    }
    mem_fifo_pool_delete(&mp);
    return 0;
}

#else
/** This one isn't actually a worst case scenario (only 2 mmap calls) because
 *  of the freepage optimisation in the fifo allocator
 *  We would need 3 pointers to have it need an allocation each time
 */
static int benchmark_fifo_worst_case(void)
{
    mem_pool_t * mp = mem_fifo_pool_new(32 * 4096);
    void *a = NULL;
    void *b = NULL;

    for (int i = 0; i < settings.num_allocs / 2; i++) {
        a = mp_new(mp, byte, 33 * 4096);
        if (b) {
            mp_ifree(mp, b);
        }
        b = mp_new(mp, byte, 256);
        mp_ifree(mp, a);
    }
    mem_fifo_pool_delete(&mp);
    return 0;
}
#endif

/** Stack allocator bench
 *
 * Runs NUM_TRIES times the function recursive_memory_user, ith a random depth
 * between 0 and MAX_DEPTH
 *
 * recursive_memory_user performs a random number of allocations between 0 and
 * MAX_ALLOCS using the stack allocator, calls itself recursively, performs
 * some allocations again and returns.
 */
static int recursive_memory_user(int depth)
{
    t_scope;
    int size = rand() % MAX(2 * settings.num_allocs /
                        (settings.num_tries * settings.max_depth), 1);
    byte ** mem = t_new_raw(byte *, size);

    for (int i = 0; i < size; i++) {
        mem[i] = t_new_raw(byte, rand() % settings.max_alloc_size);
    }

    if (likely(depth > 0)) {
        recursive_memory_user(depth - 1);
    }
    for (int i = 0; i < size; i++) {
        mem[i] = t_new_raw(byte, rand() % settings.max_alloc_size);
    }
    return 0;
}

static int benchmark_stack(void)
{
    for (int i = 0; i < settings.num_tries; i++) {
        int depth = rand() % settings.max_depth;
        recursive_memory_user(depth);
    }
    return 0;
}

static void random_recursive_func(int depth)
{
    t_scope;
    int size = rand() % (2 * settings.num_allocs /
                        (settings.num_tries * settings.max_depth));

    byte **mem = t_new_raw(byte *, size);
    int threshold = 4100;

    for (int i = 0; i < size; i++) {
        mem[i] = t_new_raw(byte, rand() % settings.max_alloc_size);
    }

  retry:
    if (depth >= settings.max_depth || rand() % 10000 < threshold) {
        return;
    } else {
        random_recursive_func(depth + 1);
        threshold += 00;
        goto retry;
    }
}

static int benchmark_stack_random(void)
{
    printf("Random bench started\n");
    random_recursive_func(0);
    return 0;
}

int main(int argc, char **argv)
{
    proctimer_t pt;
    int elapsed;
    const char *arg0 = NEXTARG(argc, argv);
    srand(time(NULL));

    argc = parseopt(argc, argv, popts, 0);
    if (argc != 0 || settings.help
    || (!settings.test_stack && !settings.test_fifo))
    {
        makeusage(0, arg0, NULL, NULL, popts);
    }

    if (settings.test_stack) {
        printf("Starting stack allocator test...\n");
        proctimer_start(&pt);
        if (settings.worst_case) {
            benchmark_stack_random();
        } else {
            benchmark_stack();
        }
        elapsed = proctimer_stop(&pt);
        printf("Stack allocator test done. Elapsed time: %d.%06d s\n",
               elapsed / 1000000, elapsed % 1000000);
    }
    if (settings.test_fifo) {
        printf("Starting fifo allocator test...\n");
        proctimer_start(&pt);
        if (settings.worst_case) {
            benchmark_fifo_worst_case();
        } else {
            benchmark_fifo();
        }
        elapsed = proctimer_stop(&pt);
        printf("Fifo allocator test done. Elapsed time: %d.%06d s\n",
               elapsed / 1000000, elapsed % 1000000);
    }
    return 0;

}

