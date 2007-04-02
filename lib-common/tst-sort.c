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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "mem.h"
#include "list.h"

#define REPEAT        10         /* randomly duplicate entries upto 4 times */
#define CHECK_STABLE  1         /* sort is not stable yet */
#define WORK_DIR      "/tmp/"   /* test file output directory */

/* Simple Linear congruential generators (LCGs):
 * next = (next * A + B) % M; rand = next / C % D;
 * Posix uses A=1103515245  B=12345  M=1<<32  C=65536  M=32768
 * Numerical recipes A=1664525, B=1013904223  M=1<<32  C=1  D=1
 * we use Posix for 15 bits and NR for 32 bits
 */
static uint32_t rand15_seed = 1;
static int rand15(void) {
    rand15_seed = rand15_seed * 1103515245 + 12345;
    return (rand15_seed >> 16) & 0x7fff;
}

static uint32_t rand32_seed = 1;
static uint32_t rand32(void) {
    return rand32_seed = rand32_seed * 1664525 + 1013904223;
}

static inline long long timeval_diff64(struct timeval *tv2, struct timeval *tv1) {
    return (tv2->tv_sec - tv1->tv_sec) * 1000000LL +
            (tv2->tv_usec - tv1->tv_usec);
}

static inline int timeval_diff(struct timeval *tv2, struct timeval *tv1) {
    return (tv2->tv_sec - tv1->tv_sec) * 1000000 +
            (tv2->tv_usec - tv1->tv_usec);
}

static inline void timer_start(struct timeval *tp) {
    gettimeofday(tp, NULL);
}

static inline long long timer_stop(struct timeval *tp) {
    struct timeval stop;
    gettimeofday(&stop, NULL);
    return timeval_diff64(&stop, tp);
}

typedef struct entry_t entry_t;

struct entry_t {
     entry_t *next;
     int number;
     int len;
     char str[1];
};

GENERIC_INIT(entry_t, entry);
GENERIC_WIPE(entry_t, entry);
GENERIC_DELETE(entry_t, entry);
SLIST_FUNCTIONS(entry_t, entry);

static int entry_number;
static int compare_number;

static inline entry_t *entry_new(const char *str, int len) {
    entry_t *ep = p_new_extra(entry_t, len);

    ep->next = NULL;
    ep->number = ++entry_number;
    ep->len = len;
    memcpy(ep->str, str, len);
    return ep;
}


static int entry_compare_dummy(const entry_t *a, const entry_t *b, void *p)
{
    compare_number++;
    return CMP(0, 1);
}

static int entry_compare_str(const entry_t *a, const entry_t *b, void *p)
{
    compare_number++;
    return strcmp(a->str, b->str);
}

static int entry_compare_str_number(const entry_t *a, const entry_t *b,
                                    void *p)
{
    int cmp;

    compare_number++;
    cmp = strcmp(a->str, b->str);
    return cmp ? cmp : CMP(a->number, b->number);
}

static int entry_compare_str_reverse(const entry_t *a, const entry_t *b,
                                     void *p)
{
    compare_number++;
    return -strcmp(a->str, b->str);
}

static int entry_compare_str_reverse_number(const entry_t *a, const entry_t *b,
    void *p)
{
    int cmp;
    
    compare_number++;
    cmp = -strcmp(a->str, b->str);
    return cmp ? cmp : CMP(a->number, b->number);
}

static int entry_compare_len(const entry_t *a, const entry_t *b, void *p)
{
    compare_number++;
    return CMP(a->len, b->len);
}

static int entry_compare_len_str_number(const entry_t *a, const entry_t *b,
                                        void *p)
{
    int cmp;

    compare_number++;
    if ((cmp = CMP(a->len, b->len)) == 0) {
        if ((cmp = strcmp(a->str, b->str)) == 0)
              return CMP(a->number, b->number);
    }
    return cmp;
}

static int entry_compare_number(const entry_t *a, const entry_t *b, void *p)
{
    compare_number++;
    return CMP(a->number, b->number);
}

static int entry_compare_random(const entry_t *a, const entry_t *b, void *p)
{
    uint32_t ia, ib, ip;

    compare_number++;
    ia = (uint32_t)(intptr_t)a->number;
    ib = (uint32_t)(intptr_t)b->number;
    ip = (uint32_t)(intptr_t)p;

    ia = (ia ^ ip) * 1125413473;
    ib = (ib ^ ip) * 987654323;

    return CMP(ia, ib);
}

typedef struct dict_t {
    entry_t *head;
    entry_t **tailp;
    int count;
} dict_t;

GENERIC_INIT(dict_t, dict);
static inline void dict_wipe(dict_t *dict)
{
    entry_t *ep;

    dict->tailp = NULL;
    while ((ep = entry_list_pop(&dict->head)) != NULL) {
        entry_delete(&ep);
    }
}

static inline void dict_append(dict_t *dict, entry_t *ep)
{
    if (!dict->tailp) {
        dict->tailp = &dict->head;
    }
    while (*dict->tailp) {
        dict->tailp = &(*dict->tailp)->next;
    }
    *dict->tailp = ep;
    dict->count++;
}

static int dict_load_file(dict_t *dict, const char *filename)
{
    char buf[BUFSIZ];
    int total_len, len, rpt;
    FILE *fp;

    if (!strcmp(filename, "-")) {
        /* BUG: -b and -t ignored for stdin */
        fp = stdin;
    } else
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "tst-sort: cannot open %s: %m\n", filename);
        exit(1);
    }
    
    total_len = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        len = strlen(buf);
        total_len += len;
        if (len && buf[len - 1] == '\n')
            buf[--len] = '\0';
#if defined(REPEAT) && REPEAT > 1
        rpt = REPEAT * rand15() / 32768;
#else
        rpt = 1;
#endif
        do {
            dict_append(dict, entry_new(buf, len));
        } while (--rpt > 0);
    }
    if (fp != stdin) {
        fclose(fp);
    }
    return total_len;
}

static void dict_dump_file(dict_t *dict, const char *filename)
{
    FILE *fp;
    entry_t *ep;

    if (!strcmp(filename, "-")) {
        /* BUG: -b and -t ignored for stdin */
        fp = stdout;
    } else
    if ((fp = fopen(filename, "w")) == NULL) {
        fprintf(stderr, "tst-sort: cannot open %s: %m\n", filename);
        exit(1);
    }
    
    for (ep = dict->head; ep; ep = ep->next) {
        fprintf(fp, "%s\t%d\n", ep->str, ep->number);
    }

    fflush(fp);
    fsync(fileno(fp));

    if (fp != stdout) {
        fclose(fp);
    }
}

static void timer_report(struct timeval *tv, const char *stage)
{
    long long elapsed = timer_stop(tv);

    if (elapsed < 0)
        elapsed = 1;

    fprintf(stderr, "%d entries %-26s in %4d.%03d ms,  %d ke/s\n",
            entry_number, stage,
            (int)(elapsed / 1000), (int)(elapsed % 1000),
            (int)((1000LL * entry_number) / elapsed));

    timer_start(tv);
}    

struct sort_test {
    const char *name;
    const char *dump_name;
    long long elapsed;
    int compare_number;
    int (*cmpf)(const entry_t *a, const entry_t *b, void *p);
    int (*checkf)(const entry_t *a, const entry_t *b, void *p);
} sort_tests[] = {
    { "minimal overhead *", WORK_DIR "w.orig", 0, 0,
    entry_compare_dummy, entry_compare_dummy },
    { "sort by line number *", NULL, 0, 0,
    entry_compare_number, entry_compare_number },
    { "sort by string *", WORK_DIR "w.str", 0, 0,
    entry_compare_str, entry_compare_str_number },
    { "sort by string length  ", WORK_DIR "w.len", 0, 0,
    entry_compare_len, entry_compare_len_str_number },
    { "sort by line number  ", WORK_DIR "w.lineno", 0, 0,
    entry_compare_number, entry_compare_number },
    { "shuffle  ", WORK_DIR "w.shuffled", 0, 0,
    entry_compare_random, NULL },
    { "sort by string  ", WORK_DIR "w.str1", 0, 0,
    entry_compare_str, entry_compare_str_number },
    { "sort by string again *", WORK_DIR "w.str2", 0, 0,
    entry_compare_str, entry_compare_str_number },
    { "sort by string reverse  ", WORK_DIR "w.rev", 0, 0,
    entry_compare_str_reverse, entry_compare_str_reverse_number },
    { NULL, NULL, 0, 0, NULL, NULL },
};

int main(int argc, char **argv)
{
    dict_t words;
    struct timeval tv;
    long long load_elapsed;
    intptr_t random_value = rand32();
    int nbytes = 0;
    int n, cmp, cmp2 = 0, status = 0, dump_files = 0;
    entry_t *ep;
    const char *name;
    struct sort_test *stp;

    dict_init(&words);

    timer_start(&tv);
    if (argc < 2) {
        nbytes += dict_load_file(&words, "/usr/share/dict/american-english");
    } else {
        while (*++argv) {
            nbytes += dict_load_file(&words, *argv);
        }
    }
    load_elapsed = timer_stop(&tv);

    for (stp = sort_tests; stp->name; stp++) {
        compare_number = 0;
        timer_start(&tv);
        entry_list_sort(&words.head, stp->cmpf, (void*)random_value);
        stp->elapsed = timer_stop(&tv);
        stp->compare_number = compare_number;

        if (stp->checkf) {
            /* Check result sequence for proper order and stability */
            int (*checkf)(const entry_t *a, const entry_t *b, void *p);
#if CHECK_STABLE
            checkf = stp->checkf;
#else
            checkf = stp->cmpf;
#endif
            for (n = 1, ep = words.head; ep->next; n++, ep = ep->next) {
                if ((*checkf)(ep, ep->next, NULL) > 0) {
                    fprintf(stderr, "%s:%d: out of order\n",
                            stp->dump_name, n + 1);
                    status = 1;
                    break;
                }
            }
        } else {
            /* just re-number entries */
            for (n = 1, ep = words.head; ep; n++, ep = ep->next) {
                ep->number = n;
            }
        }
        if (stp->dump_name && (dump_files || status)) {
            dict_dump_file(&words, stp->dump_name);
        }
    }

    /* Delay statistics output to avoid side effects on timings */
    fprintf(stderr, "%24s %4d.%03d ms, %8d lines, %d MB/s, N*log2(N)=%d\n",
            "initial load  ",
            (int)(load_elapsed / 1000), (int)(load_elapsed % 1000),
            entry_number,
            (int)((1000LL * 1000 * nbytes) / (1024 * 1024 * load_elapsed)),
            (int)(entry_number * log(entry_number) / log(2)));

    for (stp = sort_tests; stp->name; stp++) {
        fprintf(stderr, "%24s %4d.%03d ms, %8d cmps, %6d kcmp/s\n",
                stp->name,
                (int)(stp->elapsed / 1000), (int)(stp->elapsed % 1000),
                stp->compare_number,
                (int)((1000LL * stp->compare_number) / stp->elapsed));
    }

    dict_wipe(&words);

    return 0;
}
