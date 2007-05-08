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

#include <netinet/in.h>

#include "macros.h"
#include "mem.h"
#include "array.h"
#include "isndx.h"
#include "btree.h"
#include "bstream.h"
#include "timeval.h"
#include "iprintf.h"

#define USE_BSTREAM  0

#define BSWAP  1

typedef struct entry_t {
    int64_t key;
    int32_t data;
} entry_t;

GENERIC_INIT(entry_t, entry);
GENERIC_WIPE(entry_t, entry);
GENERIC_DELETE(entry_t, entry);
ARRAY_TYPE(entry_t, entry);
ARRAY_FUNCTIONS(entry_t, entry);

static int entry_cmp(const entry_t *ep1, const entry_t *ep2, void *opaque)
{
    return CMP(ep1->key, ep2->key);
}

int main(int argc, char **argv)
{
    isndx_create_parms_t cp;
    isndx_t *ndx;
    const char *filename = "/tmp/test.ndx";
    const char *command;
    int nkeys, status = 0;
    proctimer_t pt;
    struct stat st;

    if (argc < 2 || !strcmp(argv[1], "test"))
        goto do_test;

    filename = *++argv;

    ndx = isndx_open(filename, O_RDWR);
    if (!ndx) {
        printf("isndx: failed to open index file '%s'\n", filename);
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
            printf("usage: tst-isndx [{check | dump [all]}]\n");
            return 1;
        }
    }
    isndx_close(&ndx);
    return 0;

  do_test:
    proctimer_start(&pt);

#if 0
    {
        char buf[MAX_KEYLEN + 1];
        const char *dictfile = "/tmp/words";
        const char *dictfile2 = "/usr/share/dict/words";
        byte *key, *p, *data;
        int keylen, datalen, lineno = 0;
        int i, repeat;

#if USE_BSTREAM
        BSTREAM *fp;
#else
        FILE *fp;
#endif
        
#if USE_BSTREAM
        fp = bopen(dictfile, O_RDONLY);
#else
        fp = fopen(dictfile, "r");
#endif
        if (!fp) {
#if USE_BSTREAM
            fp = bopen(dictfile2, O_RDONLY);
#else
            fp = fopen(dictfile2, "r");
#endif
        }
        if (!fp) {
            printf("isndx: failed to open file '%s'\n", dictfile);
            return 1;
        }        

        cp.flags      = 0;
        cp.pageshift  = 9;
        cp.minkeylen  = 1;
        cp.maxkeylen  = MAX_KEYLEN;
        cp.mindatalen = 0;
        cp.maxdatalen = MAX_DATALEN;

        unlink(filename);
        ndx = isndx_create(filename, &cp);
        if (!ndx) {
            printf("isndx: failed to create index file '%s'\n", filename);
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
#if USE_BSTREAM
            brewind(fp);
            while (bgets(fp, buf, sizeof(buf)))
#else
                rewind(fp);
            while (fgets(buf, sizeof(buf), fp))
#endif
            {
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
#if USE_BSTREAM
        bclose(&fp);
#else
        fclose(fp);
#endif
    }
#else
    {
        int64_t start = 600000000LL;
        int32_t n, num_keys = 1000000;
        int32_t d, num_data = 4;

        cp.flags      = 0;
        cp.pageshift  = 9;
        cp.minkeylen  = 8;
        cp.maxkeylen  = 8;
        cp.mindatalen = 4;
        cp.maxdatalen = 4;

        unlink(filename);
        ndx = isndx_create(filename, &cp);
        if (!ndx) {
            printf("isndx: failed to create index file '%s'\n", filename);
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
                int64_t num = n + start;
                int32_t data = d << 10;
                byte key[8];

                if (BSWAP) {
                    num = __bswap_64(num);
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
      done:;
    }
#endif
    proctimer_stop(&pt);
    stat(filename, &st);

    printf("inserted %d keys in index '%s' (%ld bytes)\n",
           nkeys, filename, st.st_size);
    printf("times: %s\n", proctimer_report(&pt, NULL));

    if (isndx_check(ndx, ISNDX_CHECK_ALL)) {
        printf("isndx: final check failed\n");
        isndx_dump(ndx, ISNDX_DUMP_ALL, stdout);
        return 1;
    }

    isndx_close(&ndx);

    {
        int64_t start = 600000000LL;
        int32_t n, num_keys = 1000000;
        int32_t d, num_data = 4;

        btree_t *bt = NULL;
        filename = "/tmp/test.ibt";

        unlink(filename);
        bt = btree_creat(filename);
        if (!bt)
            e_panic("Cannot create %s", filename);

        proctimer_start(&pt);

        nkeys = 0;

        for (n = 0; n < num_keys; n++) {
            for (d = 0; d < num_data; d++) {
                int64_t num = n + start;
                int32_t data = d << 10;

                if (BSWAP) {
                    num = __bswap_64(num);
                }

                if (btree_push(bt, num, (void*)&data, sizeof(data))) {
                    printf("isndx: failed to insert key %lld value %d\n",
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
               nkeys, filename, st.st_size);
        printf("times: %s\n", proctimer_report(&pt, NULL));

        btree_close(&bt);
    }
    {
        int64_t start = 600000000LL;
        int32_t n, num_keys = 1000000;
        int32_t d, num_data = 4;
        entry_array entries;
        entry_t *entry_tab;

        FILE *fp;
        filename = "/tmp/test.bin";

        unlink(filename);
        fp = fopen(filename, "wb");
        if (!fp)
            e_panic("Cannot create %s", filename);

        proctimer_start(&pt);

        nkeys = 0;
        entry_tab = malloc(sizeof(entry_t) * num_keys * num_data);
        entry_array_init(&entries);
        entry_array_resize(&entries, num_keys * num_data);

        for (n = 0; n < num_keys; n++) {
            for (d = 0; d < num_data; d++) {
                int64_t num = n + start;
                int32_t data = d << 10;
                //entry_t *ep = p_new(entry_t, 1);
                entry_t *ep = &entry_tab[nkeys];

                if (BSWAP) {
                    num = __bswap_64(num);
                }

                ep->key = num;
                ep->data = data;
                //entry_array_append(&entries, ep);
                entries.tab[nkeys] = ep;
                nkeys++;
            }
        }
        entry_array_sort(&entries, entry_cmp, NULL);
        for (n = 0; n < num_keys; n++) {
            fwrite(entries.tab[n], sizeof(entry_t), 1, fp);
        }
        //entry_array_wipe(&entries, true);
        entry_array_wipe(&entries, false);
        free(entry_tab);
        p_fclose(&fp);

        proctimer_stop(&pt);
        stat(filename, &st);

        printf("inserted %d keys in index '%s' (%ld bytes)\n",
               nkeys, filename, st.st_size);
        printf("times: %s\n", proctimer_report(&pt, NULL));

    }
    return status;
}
