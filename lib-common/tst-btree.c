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

#define CHECK_LAST              0
#define INTEGRITY_CHECK_START   INT_MAX
#define INTEGRITY_CHECK_PERIOD  INT_MAX
#define EXPENSIVE_CHECK_START   INT_MAX
#define EXPENSIVE_CHECK_PERIOD  0
#define STATS_EVERY_BLOCKS 100000

#define BSWAP  1

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
            int64_t num = n + start;
            int32_t data = d << 10;

            if (bswap) {
                num = __bswap_64(num);
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
        //btree_dump(bt, fprintf, stdout);
        btree_close(&bt);
        return 1;
    }
    btree_close(&bt);
    proctimer_stop(&pt1);

    printf("    check OK (times: %s)\n", proctimer_report(&pt1, NULL));
    fflush(stdout);

    return status;
}

#if EXPENSIVE_CHECK_PERIOD
/* simple local database, 40 bytes per entry */
#define SIM_MAX_KEYS  (4096 * 1024)
typedef struct sim_record {
    uint64_t key;
    int dlen;
    byte data[32 - sizeof(int) - sizeof(uint64_t)];
} sim_record;
sim_record sim_array[SIM_MAX_KEYS];
int sim_hash[SIM_MAX_KEYS * 2];

#define SIM_HASH(key)  ((key) * 4 % (SIM_MAX_KEYS * 2 - 1))

static int sim_nbkeys;

static void sim_push(uint64_t newkey, const byte *data, int dlen)
{
    sim_record *sp;
    unsigned i;

    /* Find key in sim_array with fast hash */
    for (i = SIM_HASH(newkey);; i = (i + 1) % (SIM_MAX_KEYS * 2)) {
        if (sim_hash[i] == 0) {
            /* key not found, create slot */
            if (sim_nbkeys >= SIM_MAX_KEYS)
                return;
            sp = &sim_array[sim_nbkeys++];
            sim_hash[i] = sim_nbkeys;
            sp->key = newkey;
            sp->dlen = dlen;
            memcpy(sp->data, data, MIN(dlen, ssizeof(sp->data)));
            return;
        }
        sp = &sim_array[sim_hash[i] - 1];
        if (sp->key == newkey) {
            if (dlen < ssizeof(sp->data)) {
                memmove(sp->data + dlen, sp->data, MIN(sp->dlen, ssizeof(sp->data) - dlen));
            }
            memcpy(sp->data, data, MIN(dlen, ssizeof(sp->data)));
            sp->dlen += dlen;
            return;
        }
    }
}

static int sim_extensive_check(const btree_t *bt, long npush)
{
    int i;
    blob_t blob;

    fprintf(stderr, "Extensive check #%ld: ", npush);
    fflush(stderr);

    blob_init(&blob);
    for (i = 0; i < sim_nbkeys; i++) {
        sim_record *sp = &sim_array[i];

        blob_reset(&blob);
        if (btree_fetch(bt, sp->key, &blob) < 0) {
            fprintf(stderr, "btree_fetch(key=%04llx) failed, expected %d bytes\n",
                    (long long)sp->key, sp->dlen);
            blob_wipe(&blob);
            return 1;
        }
        if (blob.len != sp->dlen) {
            fprintf(stderr, "btree_fetch(key=%04llx) OK, got %d bytes, expected %d\n",
                    (long long)sp->key, (int)blob.len, sp->dlen);
            blob_wipe(&blob);
            return 1;
        }
        if (memcmp(blob.data, sp->data, MIN(sp->dlen, ssizeof(sp->data)))) {
            fprintf(stderr, "btree_fetch(key=%04llx) OK, got %d bytes, Content mismatch\n",
                    (long long)sp->key, (int)blob.len);
            blob_wipe(&blob);
            return 1;
        }
    }
    fprintf(stderr, "OK\n");
    blob_wipe(&blob);

    return 0;
}
#endif

static int btree_parse_test(const char *filename, const char *indexname)
{
    char buf[BUFSIZ];
    struct stat st;
    proctimer_t pt, pt2;
    btree_t *bt;
    int len;
    long offset;
    long npush;
    uint64_t num;
    uint32_t data;
    int status = 0;
    FILE *fp;
    proctimerstat_t pt_stats;

    fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "%s: cannot open %s: %m\n", __func__, filename);
        return 1;
    }

    proctimer_start(&pt);

    unlink(indexname);
    bt = btree_creat(indexname);
    if (!bt) {
        fprintf(stderr, "%s: cannot create %s: %m\n", __func__, indexname);
        return 1;
    }

    npush = 0;
    offset = 0;

    proctimer_start(&pt2);
    while (fgets(buf, sizeof(buf), fp)) {
        if (*buf == '\n')
            break;
        len = strlen(buf);
        if (buf[11] == '@' || buf[11] == '<') {
            int pos = 0, line_no, camp_id, n;

            n = buf_unpack((byte*)buf, len, &pos, "s|c|s|d|d|", NULL, NULL, NULL, &line_no, &camp_id);
            num = ((uint64_t)camp_id << 32) | (uint32_t)line_no;
            data = offset;

            npush++;
            if (npush % STATS_EVERY_BLOCKS == 0) {
                proctimer_stop(&pt2);
#if 0
                printf("btree: insertion of %d keys took %s\n",
                       STATS_EVERY_BLOCKS, proctimer_report(&pt2, NULL));
#endif
                proctimerstat_addsample(&pt_stats, &pt2);
                proctimer_start(&pt2);
            }
            if (btree_push(bt, num, (void*)&data, sizeof(data))) {
                printf("btree: failed to insert key %d:%u value %d\n",
                       camp_id, line_no, data);
                status = 1;
                break;
            }
#if INTEGRITY_CHECK_PERIOD
            if (npush >= INTEGRITY_CHECK_START
            &&  (npush - INTEGRITY_CHECK_START) % INTEGRITY_CHECK_PERIOD == 0)
            {
                if (btree_check_integrity(bt, false, fprintf, stderr)) {
                    fprintf(stderr, "Integrity check failed at npush=%ld\n", npush);
                    fprintf(stderr, "buf:%s\n", buf);
                    btree_close(&bt);
                    return 1;
                } else {
                    fprintf(stderr, "Integrity check succeeded at npush=%ld\n", npush);
                }
            }
#endif
#if EXPENSIVE_CHECK_PERIOD
            sim_push(num, (void*)&data, sizeof(data));

            if (npush >= EXPENSIVE_CHECK_START
            &&  (npush - EXPENSIVE_CHECK_START) % EXPENSIVE_CHECK_PERIOD == 0)
            {
                if (sim_extensive_check(bt, npush)) {
                    fprintf(stderr, "Error after insertion #%ld, num=%lld\n",
                            npush, (long long)num);
                    fprintf(stderr, "buf:%s\n", buf);
                    btree_close(&bt);
                    return 1;
                }
            }
#endif
            if (CHECK_LAST && npush == CHECK_LAST) {
                fprintf(stderr, "early stop at #%ld, num=%lld\n",
                        npush, (long long)num);
                break;
            }
        }
        offset += len;
    }

    proctimer_stop(&pt);

    stat(indexname, &st);
    printf("tst-btree: %ld insertions for %ld bytes. Index size: %lld\n",
           npush, offset, (long long)st.st_size);
    printf("    times: %s\n", proctimer_report(&pt, NULL));

#if EXPENSIVE_CHECK_PERIOD
    proctimer_start(&pt);
    sim_extensive_check(bt, npush);
    proctimer_stop(&pt);
    printf("    times: %s\n", proctimer_report(&pt, NULL));
#endif
    printf("    integrity check: ");
    fflush(stdout);

    proctimer_start(&pt);
    if (!btree_check_integrity(bt, false, fprintf, stderr)) {
        proctimer_stop(&pt);
        printf("OK (times: %s)\n", proctimer_report(&pt, NULL));
    }
    printf("Stats for insertion of %d samples:\n%s\n",
            STATS_EVERY_BLOCKS, proctimerstat_report(&pt_stats, NULL));

    btree_close(&bt);

    return status;
}

int main(int argc, char **argv)
{
    int status = 0;

    if (argc < 2) {
        status |= btree_linear_test("/tmp/test-1.ibt", 600000000LL, 0, 1000000, 4);
        status |= btree_linear_test("/tmp/test-2.ibt", 600000000LL, 1, 1000000, 4);
        status |= btree_linear_test("/tmp/test-3.ibt", 600000000LL, 0, 4000000, 1);
        status |= btree_linear_test("/tmp/test-4.ibt", 600000000LL, 1, 4000000, 1);
        status |= btree_linear_test("/tmp/test-5.ibt", 600000000LL, 0, 4, 1000000);
        status |= btree_linear_test("/tmp/test-6.ibt", 600000000LL, 1, 4, 1000000);
        status |= btree_linear_test("/tmp/test-7.ibt", 600000000LL, 0, 1, 4000000);
        printf("\n");

        return status;
    }

    return btree_parse_test(argv[1], "/tmp/test.ibt");
}
