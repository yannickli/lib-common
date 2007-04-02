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
#include <sys/time.h>
#include <time.h>

#include "mem.h"
#include "list.h"

#define REPEAT        10         /* randomly duplicate entries upto 4 times */
#define CHECK_STABLE  1         /* sort is not stable yet */
#define WORK_DIR      "/tmp/"   /* test file output directory */

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

static int entry_number;

static inline entry_t *entry_new(const char *str, int len)
{
    entry_t *ep = p_new_extra(entry_t, len);

    ep->next = NULL;
    ep->number = ++entry_number;
    ep->len = len;
    memcpy(ep->str, str, len);
    return ep;
}

GENERIC_WIPE(entry_t, entry);
GENERIC_DELETE(entry_t, entry);

SLIST_FUNCTIONS(entry_t, entry);

static int entry_compare_str(const entry_t *a, const entry_t *b, void *p)
{
    return strcmp(a->str, b->str);
}

static int entry_compare_str_reverse(const entry_t *a, const entry_t *b, void *p)
{
    return -strcmp(a->str, b->str);
}

static int entry_compare_len(const entry_t *a, const entry_t *b, void *p)
{
    return CMP(a->len, b->len);
}

static int entry_compare_number(const entry_t *a, const entry_t *b, void *p)
{
    return CMP(a->number, b->number);
}

static int entry_compare_random(const entry_t *a, const entry_t *b, void *p)
{
    intptr_t ia = (intptr_t)a;
    intptr_t ib = (intptr_t)b;
    intptr_t ip = (intptr_t)p;

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

static void dict_load_file(dict_t *dict, const char *filename)
{
    char buf[BUFSIZ];
    int len, rpt;
    FILE *fp;

    if (!strcmp(filename, "-")) {
        /* BUG: -b and -t ignored for stdin */
        fp = stdin;
    } else
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "tst-sort: cannot open %s: %m\n", filename);
        exit(1);
    }
    
    while (fgets(buf, sizeof(buf), fp)) {
        len = strlen(buf);
        if (len && buf[len - 1] == '\n')
            buf[--len] = '\0';
#if defined(REPEAT) && REPEAT > 1
        rpt = (REPEAT * (rand() >> 16)) / ((RAND_MAX >> 16) + 1);
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
}

static void dict_print_file(dict_t *dict, const char *filename)
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

    fprintf(stderr, "%d entries %s in %d.%03d ms: %d e/s\n",
            entry_number, stage,
            (int)(elapsed / 1000), (int)(elapsed % 1000),
            (int)((1000LL * 1000 * entry_number) / elapsed));

    timer_start(tv);
}    

int main(int argc, char **argv)
{
    dict_t words;
    struct timeval tv;
    long long elapsed;
    int random_value = rand();
    int n, cmp, cmp2 = 0, status = 0;
    entry_t *ep;
    const char *name;

    dict_init(&words);

    timer_start(&tv);
    if (argc < 2) {
        dict_load_file(&words, "/usr/share/dict/american-english");
    } else {
        while (*++argv) {
            dict_load_file(&words, *argv);
        }
    }
    timer_report(&tv, "loaded");

    dict_print_file(&words, name = WORK_DIR "w.orig");
    for (n = 1, ep = words.head; ep->next; ep = ep->next) {
        ++n;
        if (ep->number > ep->next->number) {
            printf("%s:%d: out of order\n", name, n);
            status = 1;
            break;
        }
    }

    timer_start(&tv);
    entry_list_sort(&words.head, entry_compare_str, NULL);
    timer_report(&tv, "sorted");

    dict_print_file(&words, name = WORK_DIR "w.str");
    for (n = 1, ep = words.head; ep->next; ep = ep->next) {
        ++n;
        if ((cmp = strcmp(ep->str, ep->next->str)) > 0
#if CHECK_STABLE
        ||  (cmp == 0 && ep->number > ep->next->number)
#endif
           ) {
            printf("%s:%d: out of order\n", name, n);
            status = 1;
            break;
        }
    }

    timer_start(&tv);
    entry_list_sort(&words.head, entry_compare_len, NULL);
    timer_report(&tv, "sorted by length");

    dict_print_file(&words, name = WORK_DIR "w.len");
    for (n = 1, ep = words.head; ep->next; ep = ep->next) {
        ++n;
        if ((cmp = CMP(ep->len, ep->next->len)) > 0
#if CHECK_STABLE
        ||  (cmp == 0 && (cmp2 = strcmp(ep->str, ep->next->str)) > 0)
        ||  (cmp == 0 && cmp2 == 0 && ep->number > ep->next->number)
#endif
           ) {
            printf("%s:%d: out of order\n", name, n);
            status = 1;
            break;
        }
    }

    timer_start(&tv);
    entry_list_sort(&words.head, entry_compare_number, NULL);
    timer_report(&tv, "sorted by number");

    dict_print_file(&words, name = WORK_DIR "w.number");
    for (n = 1, ep = words.head; ep->next; ep = ep->next) {
        ++n;
        if (ep->number > ep->next->number) {
            printf("%s:%d: out of order\n", name, n);
            status = 1;
            break;
        }
    }

    timer_start(&tv);
    entry_list_sort(&words.head, entry_compare_random,
                    (void*)(intptr_t)random_value);
    timer_report(&tv, "shuffled");

    /* re-number */
    for (n = 1, ep = words.head; ep; ep = ep->next) {
        ep->number = n;
        ++n;
    }
    dict_print_file(&words, name = WORK_DIR "w.shuffled");

    timer_start(&tv);
    entry_list_sort(&words.head, entry_compare_str, NULL);
    timer_report(&tv, "sorted by string");

    dict_print_file(&words, name = WORK_DIR "w.str1");
    for (n = 1, ep = words.head; ep->next; ep = ep->next) {
        ++n;
        if ((cmp = strcmp(ep->str, ep->next->str)) > 0
#if CHECK_STABLE
        ||  (cmp == 0 && ep->number > ep->next->number)
#endif
           ) {
            printf("%s:%d: out of order\n", name, n);
            status = 1;
            break;
        }
    }

    timer_start(&tv);
    entry_list_sort(&words.head, entry_compare_str, NULL);
    timer_report(&tv, "sorted by string again");

    dict_print_file(&words, name = WORK_DIR "w.str2");
    for (n = 1, ep = words.head; ep->next; ep = ep->next) {
        ++n;
        if ((cmp = strcmp(ep->str, ep->next->str)) > 0
#if CHECK_STABLE
        ||  (cmp == 0 && ep->number > ep->next->number)
#endif
           ) {
            printf("%s:%d: out of order\n", name, n);
            status = 1;
            break;
        }
    }

    timer_start(&tv);
    entry_list_sort(&words.head, entry_compare_str_reverse, NULL);
    timer_report(&tv, "sorted by string reversed");

    dict_print_file(&words, name = WORK_DIR "w.rev");
    for (n = 1, ep = words.head; ep->next; ep = ep->next) {
        ++n;
        if ((cmp = strcmp(ep->str, ep->next->str)) < 0
#if CHECK_STABLE
        ||  (cmp == 0 && ep->number > ep->next->number)
#endif
           ) {
            printf("%s:%d: out of order\n", name, n);
            status = 1;
            break;
        }
    }

    dict_wipe(&words);

    return 0;
}
