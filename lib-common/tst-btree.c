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

#include "btree.h"
#include "timeval.h"

/**************************************************************************/
/* helpers                                                                */
/**************************************************************************/

#define BSWAP  1

static int btree_linear_test(void)
{
    proctimer_t pt;
    struct stat st;

    int64_t start = 600000000LL;
    int32_t n, num_keys = 5000000;
    int32_t d, num_data = 4;
    int nkeys, status = 0;

    const char *filename = "/tmp/test.ibt";
    btree_t *bt = NULL;

    proctimer_start(&pt);

    unlink(filename);
    bt = btree_creat(filename);
    if (!bt) {
        fprintf(stderr, "Cannot create %s: %m\n", filename);
        return 1;
    }

    nkeys = 0;

    for (n = 0; n < num_keys; n++) {
        for (d = 0; d < num_data; d++) {
            int64_t num = n + start;
            int32_t data = d << 10;

            if (BSWAP) {
                num = __bswap_64(num);
            }

            if (btree_push(bt, num, (void*)&data, sizeof(data))) {
                printf("btree: failed to insert key %lld value %d\n",
                       (long long)num, data);
                status = 1;
                goto done1;
            }
            nkeys++;
        }
    }
  done1:
    proctimer_stop(&pt);
    stat(filename, &st);

    printf("inserted %d keys in index '%s' (%ld bytes)\n",
           nkeys, filename, (long)st.st_size);
    printf("times: %s\n", proctimer_report(&pt, NULL));

    btree_close(&bt);

    return status;
}

#define EXPENSIVE_CHECKS
static int do_expensive_checks(const btree_t *bt, uint64_t newkey, long npush)
{
    static uint64_t keys[1024 * 1024] = {0};
    static int nb[1024 * 1024] = {0};
    static int nb_keys = 0;
    int i;
    blob_t blob;

    /* Find previous occurence of newkey in keys[].
     * Do it in reverse order as it's statistically more
     * likely */
    for (i = nb_keys - 1; i >= 0; i--) {
        if (keys[i] == newkey) {
            break;
        }
    }
    if (i >= 0) {
        nb[i]++;
    } else {
        keys[nb_keys] = newkey;
        nb[nb_keys] = 1;
        nb_keys++;
    }

    if (npush >= 335500) {
        fprintf(stderr, "Expensive check #%ld...", npush);
        fflush(stderr);
        blob_init(&blob);
        for (i = 0; i < nb_keys; i++) {
            blob_reset(&blob);
            btree_fetch(bt, keys[i], &blob);
            if (blob.len != 4 * nb[i]) {
                fprintf(stderr, "looking for key=%04lx. Got %d, expected %d\n",
                        keys[i],
                        (int)blob.len,
                        4 * nb[i]);
                blob_wipe(&blob);
                return 1;
            }
        }
        fprintf(stderr, "OK\n");
        blob_wipe(&blob);
    }
    return 0;
}
int main(int argc, char **argv)
{
    char buf[BUFSIZ];
    const char *indexname = "/tmp/test.ibt";
    btree_t *bt = NULL;
    int len;
    long offset;
    long npush;
    uint64_t num;
    uint32_t data;
    int status = 0;
    FILE *fp;

    if (argc < 2) {
        return btree_linear_test();
    }

    fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Cannot open %s: %m\n", argv[1]);
        return 1;
    }

    unlink(indexname);
    bt = btree_creat(indexname);
    if (!bt) {
        fprintf(stderr, "Cannot create %s: %m\n", indexname);
        return 1;
    }

    npush = 0;
    offset = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        if (*buf == '\n')
            break;
        len = strlen(buf);
        if (buf[11] == '@' || buf[11] == '<') {
            int pos = 0, line_no, camp_id, n;

            n = buf_unpack((byte*)buf, len, &pos, "s|c|s|d|d|", NULL, NULL, NULL, &line_no, &camp_id);
            num = ((uint64_t)camp_id << 32) | (uint32_t)line_no;
            data = offset;
            if (btree_push(bt, num, (void*)&data, sizeof(data))) {
                printf("btree: failed to insert key %lld value %d\n",
                       (long long)num, data);
                status = 1;
                break;
            }
            npush++;
#ifdef EXPENSIVE_CHECKS
            if (do_expensive_checks(bt, num, npush)) {
                fprintf(stderr, "Error after insertion #%ld\n"
                        "num=%lld\n", npush,
                        (long long unsigned int)num);
                fprintf(stderr, "buf:%s\n", buf);
                btree_close(&bt);
                return 1;
            }
#endif
        }
        offset += len;
        if (npush == 335505) {
            btree_close(&bt);
            return 0;
        }
    }
    btree_close(&bt);

    printf("btree: %ld insertions for %ld bytes\n",
           npush, offset);

    return status;
}

