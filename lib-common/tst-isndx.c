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
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>

#include "macros.h"
#include "mem.h"
#include "array.h"
#include "isndx.h"
#include "btree.h"
#include "bstream.h"
#include "timeval.h"
#include "iprintf.h"

#define USE_BSTREAM  0

typedef struct entry_t {
    int64_t key;
    int32_t data;
} entry_t;

GENERIC_INIT(entry_t, entry);
GENERIC_WIPE(entry_t, entry);
GENERIC_DELETE(entry_t, entry);
ARRAY_TYPE(entry_t, entry);
ARRAY_FUNCTIONS(entry_t, entry, entry_delete);

static int entry_cmp(const entry_t *ep1, const entry_t *ep2, void *opaque)
{
    return CMP(ep1->key, ep2->key);
}

static int array_linear_test(const char *indexname, int64_t start, int bswap,
                             int32_t num_keys, int32_t num_data)
{
    struct stat st;
    proctimer_t pt;
    int nkeys, status = 0;
    entry_array entries;
    entry_t *entry_tab;
    int32_t n, d;

    FILE *fp;

    unlink(indexname);
    fp = fopen(indexname, "wb");
    if (!fp) {
        fprintf(stderr, "%s: cannot create %s: %m\n", __func__, indexname);
        return 1;
    }

    proctimer_start(&pt);

    nkeys = 0;
    entry_tab = malloc(sizeof(entry_t) * num_keys * num_data);
    entry_array_init(&entries);
    entry_array_resize(&entries, num_keys * num_data);

    for (n = 0; n < num_keys; n++) {
        for (d = 0; d < num_data; d++) {
            int64_t num = start + n;
            int32_t data = d << 10;
            //entry_t *ep = p_new(entry_t, 1);
            entry_t *ep = &entry_tab[nkeys];

            if (bswap) {
                num = start + __bswap_32(n);
            }

            ep->key = num;
            ep->data = data;
            //entry_array_append(&entries, ep);
            entries.tab[nkeys] = ep;
            nkeys++;
        }
    }
    entry_array_sort(&entries, entry_cmp, NULL);
    for (n = 0; n < nkeys; ) {
#if 1
        int n1, nb;
        int64_t key = entries.tab[n]->key;
        for (nb = 4, n1 = n + 1; n1 < nkeys; n1++, nb += 4) {
            if (nb >= 256 - 4
            ||  entries.tab[n1]->key != key)
                break;
        }
        putc_unlocked(8, fp);
        fwrite_unlocked(&key, sizeof(int64_t), 1, fp);
        putc_unlocked(nb, fp);
        while (n < n1) {
            fwrite_unlocked(&entries.tab[n]->data, sizeof(int32_t), 1, fp);
            n++;
        }
#else
        fwrite_unlocked(entries.tab[n], sizeof(entry_t), 1, fp);
        n++;
#endif
    }
    proctimer_stop(&pt);

    //entry_array_wipe(&entries, true);
    entry_array_resize(&entries, 0);
    entry_array_wipe(&entries);
    free(entry_tab);
    p_fclose(&fp);

    proctimer_stop(&pt);
    stat(indexname, &st);

    printf("%s: %s: %d keys, %d chunks,%s %lld bytes\n",
           __func__, indexname, num_keys, num_data,
           bswap ? " bswap" : "", (long long)st.st_size);
    printf("    times: %s\n", proctimer_report(&pt, NULL));
    fflush(stdout);

    return 0;
}

#define MAX_KEYLEN   255
#define MAX_DATALEN  255

#if !USE_BSTREAM
#define BSTREAM          FILE
#define bopen(fn, mode)  fopen(fn, "r")
#define brewind(s)       rewind(s)
#define bgets(s,b,n)     fgets(b,n,s)
#define bclose(sp)       (*(sp) ? (fclose(*(sp)), *(sp) = NULL) : 0)
#endif

#if 1
static int isndx_word_test(const char *indexname)
{
    struct stat st;
    proctimer_t pt, pt1;
    int nkeys, status = 0;

    isndx_create_parms_t cp;
    isndx_t *ndx;

    char buf[MAX_KEYLEN + 1];
    const char *dictfile = "/tmp/words";
    const char *dictfile2 = "/usr/share/dict/words";
    byte *key, *p, *data;
    int keylen, datalen, lineno = 0;
    int i, repeat;
    BSTREAM *fp;

    fp = bopen(dictfile, O_RDONLY);

    if (!fp) {
        fp = bopen(dictfile2, O_RDONLY);
    }
    if (!fp) {
        printf("isndx: failed to open file '%s'\n", dictfile);
        return 1;
    }

    cp.flags      = 0;
    cp.pageshift  = 0;
    cp.minkeylen  = 1;
    cp.maxkeylen  = MAX_KEYLEN;
    cp.mindatalen = 0;
    cp.maxdatalen = MAX_DATALEN;

    proctimer_start(&pt);

    unlink(indexname);
    ndx = isndx_create(indexname, &cp);
    if (!ndx) {
        fprintf(stderr, "%s: cannot create %s: %m\n", __func__, indexname);
        return 1;
    }

    isndx_set_error_stream(ndx, stdout);

    if (isndx_check(ndx, ISNDX_CHECK_ALL)) {
        printf("isndx: initial check failed\n");
        isndx_dump(ndx, ISNDX_DUMP_ALL, stdout);
        return 1;
    }

    nkeys = 0;

    repeat = 300;
    for (i = 0; i < repeat; i++) {
        brewind(fp);
        while (bgets(fp, buf, sizeof(buf))) {
            lineno++;
#if 1
            for (key = (byte*)buf; isspace(*key); key++)
                continue;
            for (p = key; *p && !isspace(*p); p++)
                continue;
            keylen = p - key;
#else
            key = (byte *)buf;
            keylen = strlen(buf);
#endif
            //key[keylen] = key[0];
            //key++;
            //key[keylen++] = 'a' + i;
            if (keylen) {
                data = (byte*)&lineno;
                datalen = sizeof(lineno);
                while (datalen && data[datalen - 1] == 0) {
                    datalen--;
                }
                if (isndx_push(ndx, key, keylen, data, datalen) < 0) {
                    printf("isndx: failed to insert key \"%.*s\" value %d\n",
                           keylen, key, lineno);
                    isndx_check(ndx, ISNDX_CHECK_ALL);
                    isndx_dump(ndx, ISNDX_DUMP_ALL, stdout);
                    status = 1;
                    break;
                }
#if 0
                if (isndx_check(ndx, ISNDX_CHECK_ALL)) {
                    printf("isndx: check failed after insert key \"%.*s\" value %d\n",
                           keylen, key, lineno);
                    isndx_dump(ndx, ISNDX_DUMP_ALL, stdout);
                    return 1;
                }
#endif
                nkeys++;
            }
        }
    }
    proctimer_stop(&pt);
    stat(indexname, &st);

    printf("%s: %s: %d keys, %d chunks, %lld bytes\n",
           __func__, indexname, nkeys / repeat, repeat,
           (long long)st.st_size);

    printf("    times: %s\n", proctimer_report(&pt, NULL));
    fflush(stdout);

    proctimer_start(&pt1);
    if (isndx_check(ndx, ISNDX_CHECK_ALL)) {
        printf("    final check failed\n");
        isndx_dump(ndx, ISNDX_DUMP_ALL, stdout);
        isndx_close(&ndx);
        return 1;
    }
    isndx_close(&ndx);
    proctimer_stop(&pt1);

    printf("    check OK (times: %s)\n", proctimer_report(&pt1, NULL));
    fflush(stdout);

    bclose(&fp);

    return status;
}
#endif

static int isndx_linear_test(const char *indexname, int64_t start, int bswap,
                             int32_t num_keys, int32_t num_data)
{
    struct stat st;
    proctimer_t pt, pt1;
    int nkeys, status = 0;
    isndx_create_parms_t cp;
    isndx_t *ndx;
    int32_t n, d;

    cp.flags      = 0;
    cp.pageshift  = 0;
    cp.minkeylen  = 8;
    cp.maxkeylen  = 8;
    cp.mindatalen = 4;
    cp.maxdatalen = 4;

    proctimer_start(&pt);

    unlink(indexname);
    ndx = isndx_create(indexname, &cp);
    if (!ndx) {
        fprintf(stderr, "%s: cannot create %s: %m\n", __func__, indexname);
        return 1;
    }

    isndx_set_error_stream(ndx, stdout);

    if (isndx_check(ndx, ISNDX_CHECK_ALL)) {
        printf("isndx: initial check failed\n");
        isndx_dump(ndx, ISNDX_DUMP_ALL, stdout);
        return 1;
    }

    nkeys = 0;

    for (n = 0; n < num_keys; n++) {
        for (d = 0; d < num_data; d++) {
            int64_t num = start + n;
            int32_t data = d << 10;
            byte key[8];

            if (bswap) {
                num = start + __bswap_32(n);
            }

            key[0] = num >> (7 * 8);
            key[1] = num >> (6 * 8);
            key[2] = num >> (5 * 8);
            key[3] = num >> (4 * 8);
            key[4] = num >> (3 * 8);
            key[5] = num >> (2 * 8);
            key[6] = num >> (1 * 8);
            key[7] = num >> (0 * 8);
            if (isndx_push(ndx, key, 8, &d, sizeof(d)) < 0) {
                printf("isndx: failed to insert key %lld value %d\n",
                       (long long)num, data);
                isndx_check(ndx, ISNDX_CHECK_ALL);
                isndx_dump(ndx, ISNDX_DUMP_ALL, stdout);
                status = 1;
                goto done;
            }
            nkeys++;
        }
    }
  done:
    proctimer_stop(&pt);
    stat(indexname, &st);

    printf("%s: %s: %d keys, %d chunks,%s %lld bytes\n",
           __func__, indexname, num_keys, num_data,
           bswap ? " bswap" : "", (long long)st.st_size);
    printf("    times: %s\n", proctimer_report(&pt, NULL));
    fflush(stdout);

    proctimer_start(&pt1);
    if (isndx_check(ndx, ISNDX_CHECK_ALL)) {
        printf("    final check failed\n");
        isndx_dump(ndx, ISNDX_DUMP_ALL, stdout);
        isndx_close(&ndx);
        return 1;
    }
    isndx_close(&ndx);
    proctimer_stop(&pt1);

    printf("    check OK (times: %s)\n", proctimer_report(&pt1, NULL));
    fflush(stdout);

    unlink(indexname);
    return status;
}

static int btree_linear_test(const char *indexname, int64_t start, int bswap,
                             int32_t num_keys, int32_t num_data)
{
    struct stat st;
    proctimer_t pt, pt1;
    int nkeys, status = 0;
    btree_t *bt;
    int32_t n, d;

    unlink(indexname);
    bt = btree_creat(indexname);
    if (!bt) {
        fprintf(stderr, "%s: cannot create %s: %m\n", __func__, indexname);
        return 1;
    }

    proctimer_start(&pt);

    nkeys = 0;

    for (n = 0; n < num_keys; n++) {
        for (d = 0; d < num_data; d++) {
            int64_t num = start + n;
            int32_t data = d << 10;

            if (bswap) {
                num = start + __bswap_32(n);
            }

            if (btree_push(bt, num, (void*)&data, sizeof(data))) {
                fprintf(stderr, "btree: failed to insert key %lld value %d\n",
                        (long long)num, data);
                status = 1;
                goto done;
            }
            nkeys++;
        }
    }
  done:
    proctimer_stop(&pt);
    stat(indexname, &st);

    printf("%s: %s: %d keys, %d chunks,%s %lld bytes\n",
           __func__, indexname, num_keys, num_data,
           bswap ? " bswap" : "", (long long)st.st_size);
    printf("    times: %s\n", proctimer_report(&pt, NULL));
    fflush(stdout);

    proctimer_start(&pt1);
    if (btree_check_integrity(bt, false, fprintf, stderr)) {
        printf("    final integrity check failed\n");
        btree_dump(bt, fprintf, stdout);
        btree_close(&bt);
        return 1;
    }
    btree_close(&bt);
    proctimer_stop(&pt1);

    printf("    check OK (times: %s)\n", proctimer_report(&pt1, NULL));
    fflush(stdout);

    return status;
}

static int benchmark_index_methods(void)
{
    int status = 0;

#if 0
    status |= isndx_word_test("/tmp/words.ndx");
#endif
    printf("benchmarking in increasing order, %d * %d\n\n", 1000000, 4);
    status |= isndx_linear_test("/tmp/test-1.ndx", 600000000LL, 0, 1000000, 4);
    status |= btree_linear_test("/tmp/test-1.ibt", 600000000LL, 0, 1000000, 4);
    status |= array_linear_test("/tmp/test-1.bin", 600000000LL, 0, 1000000, 4);
    printf("\n");

    status |= isndx_linear_test("/tmp/test-2.ndx", 600000000LL, 1, 1000000, 4);
    status |= btree_linear_test("/tmp/test-2.ibt", 600000000LL, 1, 1000000, 4);
    status |= array_linear_test("/tmp/test-2.bin", 600000000LL, 1, 1000000, 4);
    printf("\n");

    status |= isndx_linear_test("/tmp/test-3.ndx", 600000000LL, 0, 4000000, 1);
    status |= btree_linear_test("/tmp/test-3.ibt", 600000000LL, 0, 4000000, 1);
    status |= array_linear_test("/tmp/test-3.bin", 600000000LL, 0, 4000000, 1);
    printf("\n");

    status |= isndx_linear_test("/tmp/test-4.ndx", 600000000LL, 1, 4000000, 1);
    status |= btree_linear_test("/tmp/test-4.ibt", 600000000LL, 1, 4000000, 1);
    status |= array_linear_test("/tmp/test-4.bin", 600000000LL, 1, 4000000, 1);
    printf("\n");

    status |= isndx_linear_test("/tmp/test-5.ndx", 600000000LL, 0, 4, 1000000);
    status |= btree_linear_test("/tmp/test-5.ibt", 600000000LL, 0, 4, 1000000);
    status |= array_linear_test("/tmp/test-5.bin", 600000000LL, 0, 4, 1000000);
    printf("\n");

    status |= isndx_linear_test("/tmp/test-6.ndx", 600000000LL, 1, 4, 1000000);
    status |= btree_linear_test("/tmp/test-6.ibt", 600000000LL, 1, 4, 1000000);
    status |= array_linear_test("/tmp/test-6.bin", 600000000LL, 1, 4, 1000000);
    printf("\n");

    status |= isndx_linear_test("/tmp/test-7.ndx", 600000000LL, 0, 1, 4000000);
    status |= btree_linear_test("/tmp/test-7.ibt", 600000000LL, 0, 1, 4000000);
    status |= array_linear_test("/tmp/test-7.bin", 600000000LL, 0, 1, 4000000);
    printf("\n");

    return status;
}

int main(int argc, char **argv)
{
    isndx_create_parms_t cp;
    isndx_t *ndx;
    const char *indexname;
    const char *command;
    int status = 0;

    if (argc < 2) {
        status |= isndx_linear_test("/tmp/test-1.ndx", 600000000LL, 0, 1000000, 4);
        status |= isndx_linear_test("/tmp/test-2.ndx", 600000000LL, 1, 1000000, 4);
        status |= isndx_linear_test("/tmp/test-3.ndx", 600000000LL, 0, 4000000, 1);
        status |= isndx_linear_test("/tmp/test-4.ndx", 600000000LL, 1, 4000000, 1);
        status |= isndx_linear_test("/tmp/test-5.ndx", 600000000LL, 0, 4, 1000000);
        status |= isndx_linear_test("/tmp/test-6.ndx", 600000000LL, 1, 4, 1000000);
        status |= isndx_linear_test("/tmp/test-7.ndx", 600000000LL, 0, 1, 4000000);
        printf("\n");

        return status;
    }

    if (!strcmp(argv[1], "compare")) {
        return benchmark_index_methods();
    }

    indexname = *++argv;
    ndx = isndx_open(indexname, O_RDWR);
    if (!ndx) {
        printf("isndx: failed to open index file '%s'\n", indexname);
        return 1;
    }

    isndx_set_error_stream(ndx, stdout);

    if (!*++argv) {
        isndx_check(ndx, ISNDX_CHECK_ALL);
    } else {
        while ((command = *argv++) != NULL) {
            if (!strcmp(command, "check")) {
                isndx_check(ndx, ISNDX_CHECK_ALL);
                continue;
            }
            if (!strcmp(command, "dump")) {
                if (!*argv) {
                    isndx_dump(ndx, 0, stdout);
                } else
                if (!strcmp(*argv, "all")) {
                    argv++;
                    isndx_dump(ndx, ISNDX_DUMP_ALL, stdout);
                } else
                if (!strcmp(*argv, "pages")) {
                    argv++;
                    isndx_dump(ndx, ISNDX_DUMP_PAGES, stdout);
                } else {
                    uint32_t pageno;
                    isndx_dump(ndx, 0, stdout);
                    while (*argv && (pageno = strtoul(*argv++, NULL, 0)) > 0) {
                        isndx_dump_page(ndx, pageno, ISNDX_DUMP_ALL, stdout);
                    }
                }
                continue;
            }
            printf("usage: tst-isndx\n"
                   "       tst-isndx compare\n"
                   "       tst-isndx indexfile [{check | dump [all | pages]}]\n");
            return 1;
        }
    }
    isndx_close(&ndx);
    return 0;
}
