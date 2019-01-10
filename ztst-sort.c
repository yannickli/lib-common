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

#include <math.h>

#include "time.h"
#include "container.h"

#define REPEAT        10        /* randomly duplicate entries upto 4 times */
#define WORK_DIR      "/tmp/"   /* test file output directory */

static const char *default_dictionary = "/usr/share/dict/american-english";
static const char *default_dictionary_package = "wamerican";

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

typedef struct entry_t entry_t;

struct entry_t {
    struct slist_head entry_list;
    int number;
    int len;
    char str[1];
};

GENERIC_INIT(entry_t, entry);
GENERIC_WIPE(entry_t, entry);
GENERIC_DELETE(entry_t, entry);
ARRAY_TYPE(entry_t, entry);
ARRAY_FUNCTIONS(entry_t, entry, entry_delete);

static int entry_number;
static int compare_number;
static int max_entry = INT_MAX;
static int repeat_entry = REPEAT;

static entry_t *entry_new(const char *str, int len)
{
    entry_t *ep = p_new_extra(entry_t, len);
    ep->number = ++entry_number;
    ep->len = len;
    memcpy(ep->str, str, len);
    return ep;
}


static int entry_compare_dummy(const entry_t *a, const entry_t *b, void *p)
{
    compare_number++;
    return 0;
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

#if 0
static int entry_compare_str_number_reverse(const entry_t *a, const entry_t *b,
                                            void *p)
{
    int cmp;

    compare_number++;
    cmp = strcmp(a->str, b->str);
    return -(cmp ? cmp : CMP(a->number, b->number));
}
#endif

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

static int entry_compare_number_reverse(const entry_t *a, const entry_t *b, void *p)
{
    compare_number++;
    return -CMP(a->number, b->number);
}

static int entry_compare_random(const entry_t *a, const entry_t *b, void *p)
{
    uint32_t ia, ib, ip;

    compare_number++;
    ia = (uint32_t)(intptr_t)a->number;
    ib = (uint32_t)(intptr_t)b->number;
    ip = (uint32_t)(intptr_t)p;

    ia = (ia ^ ip) * 1125413473;
    ib = (ib ^ ip) * 1125413473;

    return CMP(ia, ib);
}

typedef struct dict_t {
    entry_array entries;
    struct slist_head head, *tailp;
    int count;
} dict_t;

static dict_t *dict_init(dict_t *dict)
{
    p_clear(dict, 1);
    entry_array_init(&dict->entries);
    return dict;
}

static void dict_wipe(dict_t *dict)
{
    entry_t *ep;

    entry_array_wipe(&dict->entries);

    dict->tailp = NULL;
    slist_for_each_entry_safe(ep, prev, &dict->head, entry_list) {
        slist_pop(prev);
        entry_delete(&ep);
    }
}

static void dict_append(dict_t *dict, entry_t *ep)
{
    entry_array_append(&dict->entries, ep);

    if (!dict->tailp) {
        dict->tailp = &dict->head;
    }
    slist_append(slist_tail(&dict->tailp), &ep->entry_list);
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
    while (entry_number < max_entry && fgets(buf, sizeof(buf), fp)) {
        len = strlen(buf);
        total_len += len;
        if (len && buf[len - 1] == '\n')
            buf[--len] = '\0';

        rpt = (repeat_entry > 1) ? repeat_entry * rand15() / 32768 : 1;
        do {
            if (entry_number >= max_entry)
                break;
            dict_append(dict, entry_new(buf, len));
        } while (--rpt > 0);
    }
    if (fp != stdin) {
        fclose(fp);
    }
    return total_len;
}

static void dict_dump_file(dict_t *dict, const char *filename, bool fromlist)
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

    if (fromlist) {
        slist_for_each_entry(ep, &dict->head, entry_list) {
            fprintf(fp, "%s\t%d\n", ep->str, ep->number);
        }
    } else {
        int n;

        for (n = 0; n < dict->entries.len; n++) {
            ep = dict->entries.tab[n];
            fprintf(fp, "%s\t%d\n", ep->str, ep->number);
        }
    }

    fflush(fp);
    fsync(fileno(fp));

    if (fp != stdout) {
        fclose(fp);
    }
}

struct sort_test {
    const char *name;
    const char *dump_name;
    long long elapsed;
    int compare_number;
    int (*cmpf)(const entry_t *a, const entry_t *b, void *p);
    int (*checkf)(const entry_t *a, const entry_t *b, void *p);
} sort_tests[] = {
    { "number entries  ", NULL, 0, 0,
        NULL, NULL },
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
    { "sort by reverse lineno R", WORK_DIR "w.rev1", 0, 0,
        entry_compare_number_reverse, entry_compare_number_reverse },
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

/* complexity of standard merge sort */
static double cmerge1(size_t n, int c)
{
    switch (n) {
      case 0:
      case 1:
        return 0;
      case 2:
        return 1;
      case 3:
        return c == 0 ? 2 : c == 1 ? 2.5 : 3;
      case 4:
        return 5;
      default:
        return cmerge1(n >> 1, c) + cmerge1(n - (n >> 1), c) + n - 1;
    }
}

/* complexity of skewed merge sort with ordered subsequence check */
static double cmerge2(size_t n, int c)
{
    switch (n) {
      case 0:
      case 1:
        return 0;
      case 2:
        return 1;
      case 3:
        return c == 0 ? 2 : c == 1 ? 2.5 : 3;
      case 4:
        return c == 0 ? 3 : c == 1 ? 35.0 / 6 : 7;
      default:
        return cmerge2(n >> 1, c) + cmerge2(n - (n >> 1), c) +
            (c == 0 ? 1 : 2 + n - 1);
    }
}

static int ilog2(size_t n)
{
    int i;
    for (i = 0; n > 0; i++, n >>= 1)
        continue;
    return i;
}

/* complexity of optimized merge sort with ordered subsequence check
 * and power of 2 subsequences and binary search threshold test.
 */
static double cmerge3(size_t n, int c)
{
    switch (n) {
      case 0:
      case 1:
        return 0;
      case 2:
        return 1;
      case 3:
        return c == 0 ? 2 : c == 1 ? 2.5 : 3;
      case 4:
        return c == 0 ? 3 : c == 1 ? 35.0 / 6 : 7;
      default:
        if (n & (n - 1)) {
            size_t n1, n2, i, rec;

            /* compute left part: largest block with size power of 2 */
            for (n1 = n - 1; n1 & (n1 - 1); n1 &= n1 - 1)
                continue;
            n2 = n - n1;
            rec = cmerge3(n1, c) + cmerge3(n2, c);
            if (c == 0) {
                return rec + 1;
            }
            i = (n1 + n2) / n2;
            if (i > 30 || (1U << i) > n1 + 1) {
                /* For n2 much smaller than n1, use binary searches */
                return rec + 2 + ilog2(n1) * n2;
            } else {
                /* Else use standard merge with n-1 comparisons */
                return rec + 2 + n - 1;
            }
        } else {
            return 2 * cmerge3(n >> 1, c) + (c == 0 ? 1 : 2 + n - 1);
        }
    }
}

static double nlog2n(size_t n)
{
    return n ? (n * log(n) / log(2)) : 0;
}

int main(int argc, char **argv)
{
    dict_t words;
    proctimer_t pt;
    long long load_elapsed, total_elapsed, total_compare;
    intptr_t random_value = rand32();
    int nbytes = 0;
    int n, status = 0, dump_files = 0;
    entry_t *ep;
    struct sort_test *stp;

    dict_init(&words);

    proctimer_start(&pt);

    if (argv[1] && isdigit((unsigned char)*argv[1])) {
        repeat_entry = strtol(*++argv, NULL, 0);
        argc--;
        if (argv[1] && isdigit((unsigned char)*argv[1])) {
            max_entry = strtol(*++argv, NULL, 0);
            argc--;
        }
    }

    if (argc < 2) {
        if (access(default_dictionary, R_OK)) {
            fprintf(stderr,
                    "tst-sort: dictionary file %s not installed,\n"
                    "    install package %s or pass dictionary on command line\n",
                    default_dictionary, default_dictionary_package);
            exit(1);
        }
        nbytes += dict_load_file(&words, default_dictionary);
    } else {
        while (*++argv) {
            nbytes += dict_load_file(&words, *argv);
        }
    }
    load_elapsed = proctimer_stop(&pt);

    /* Do the tests with list_sort */
    total_elapsed = total_compare = 0;
    for (stp = sort_tests; stp->name; stp++) {
        if (stp->cmpf) {
            compare_number = 0;
            proctimer_start(&pt);
            slist_sort(&words.head, stp->cmpf, (void*)random_value,
                       entry_t, entry_list);
            total_elapsed += stp->elapsed = proctimer_stop(&pt);
            total_compare += stp->compare_number = compare_number;
        }
        if (words.head.next) {
            if (stp->checkf) {
                /* Check result sequence for proper order and stability */
                int (*checkf)(const entry_t *a, const entry_t *b, void *p) =
                    LIST_SORT_IS_STABLE ? stp->checkf : stp->cmpf;

                n = 1;
                slist_for_each_entry(ep, &words.head, entry_list) {
                    entry_t *epnext = slist_next_entry(ep, entry_list);
                    if (!epnext)
                        break;
                    if ((*checkf)(ep, epnext, NULL) > 0) {
                        fprintf(stderr, "%s:%d: out of order\n",
                                stp->dump_name, n + 1);
                        status = 1;
                        break;
                    }
                    n++;
                }
                if (n != entry_number && !slist_next_entry(ep, entry_list)) {
                    fprintf(stderr, "%s:%d: missing entry\n",
                            stp->dump_name, n + 1);
                    status = 1;
                }
            } else {
                /* just re-number entries */
                n = 0;
                slist_for_each_entry(ep, &words.head, entry_list) {
                    ep->number = ++n;
                }
            }
        }
        if (stp->dump_name && (dump_files || status)) {
            dict_dump_file(&words, stp->dump_name, true);
        }
    }

    n = entry_number;

    /* Delay statistics output to avoid side effects on timings */
    fprintf(stderr, "%24s %5d.%03d ms, %9d lines, %5d KB/s\n"
            "%24s %12s  %9.0f cmps\n"
            "%24s %12s  %9.0f cmps [%9.0f %9.0f]\n"
            "%24s %12s  %9.0f cmps [%9.0f %9.0f]\n"
            "%24s %12s  %9.0f cmps [%9.0f %9.0f]\n",
            "initial load  ",
            (int)(load_elapsed / 1000), (int)(load_elapsed % 1000),
            n,
            (int)((1000LL * 1000 * nbytes) / (1024 * load_elapsed)),
            "complexity  ", "N*lg2(N/2)+1", nlog2n(n) - n + 1,
            "standard mergesort  ", "cmerge1(N)", cmerge1(n, 1), cmerge1(n, 0), cmerge1(n, 2),
            "skewed mergesort  ", "cmerge2(N)", cmerge2(n, 1), cmerge2(n, 0), cmerge2(n, 2),
            "optimized mergesort  ", "cmerge3(N)", cmerge3(n, 1), cmerge3(n, 0), cmerge3(n, 2));
    fprintf(stderr, "\n");

    for (stp = sort_tests; stp->name; stp++) {
        if (stp->cmpf) {
            fprintf(stderr, "%24s %5d.%03d ms, %9d cmps, %6d kcmp/s\n",
                    stp->name,
                    (int)(stp->elapsed / 1000), (int)(stp->elapsed % 1000),
                    stp->compare_number,
                    (int)((1000LL * stp->compare_number) / stp->elapsed));
        }
    }
    fprintf(stderr, "%24s %5d.%03d ms, %9lld cmps, %6d kcmp/s\n",
            "total =",
            (int)(total_elapsed / 1000), (int)(total_elapsed % 1000),
            total_compare,
            (int)((1000LL * total_compare) / total_elapsed));
    fprintf(stderr, "\n");

#ifdef ARRAY_SORT_IS_STABLE
    /* Do the tests with array_sort */
    total_elapsed = total_compare = 0;
    for (stp = sort_tests; stp->name; stp++) {
        if (stp->cmpf) {
            compare_number = 0;
            proctimer_start(&pt);
            entry_array_sort(&words.entries, stp->cmpf, (void*)random_value);
            total_elapsed += stp->elapsed = proctimer_stop(&pt);
            total_compare += stp->compare_number = compare_number;
        }
        if (stp->checkf) {
            /* Check result sequence for proper order and stability */
            int (*checkf)(const entry_t *a, const entry_t *b, void *p) =
                ARRAY_SORT_IS_STABLE ? stp->checkf : stp->cmpf;

            for (n = 1; n < words.entries.len; n++) {
                if ((*checkf)(words.entries.tab[n - 1],
                              words.entries.tab[n], NULL) > 0) {
                    fprintf(stderr, "%s:%d: out of order\n",
                            stp->dump_name, n + 1);
                    status = 1;
                    break;
                }
            }
            if (words.entries.len != entry_number) {
                fprintf(stderr, "%s:%d: missing entry\n",
                        stp->dump_name, words.entries.len);
                status = 1;
            }
        } else {
            /* just re-number entries */
            for (n = 0; n < words.entries.len; n++) {
                words.entries.tab[n]->number = n + 1;
            }
        }
        if (stp->dump_name && (dump_files || status)) {
            dict_dump_file(&words, stp->dump_name, false);
        }
    }
    for (stp = sort_tests; stp->name; stp++) {
        if (stp->cmpf) {
            fprintf(stderr, "%24s %5d.%03d ms, %9d cmps, %6d kcmp/s\n",
                    stp->name,
                    (int)(stp->elapsed / 1000), (int)(stp->elapsed % 1000),
                    stp->compare_number,
                    (int)((1000LL * stp->compare_number) / stp->elapsed));
        }
    }
    fprintf(stderr, "%24s %5d.%03d ms, %9lld cmps, %6d kcmp/s\n",
            "total =",
            (int)(total_elapsed / 1000), (int)(total_elapsed % 1000),
            total_compare,
            (int)((1000LL * total_compare) / total_elapsed));
    fprintf(stderr, "\n");
#endif

    dict_wipe(&words);

    return 0;
}
