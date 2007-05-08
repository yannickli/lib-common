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

#include "btree.h"
#include "timeval.h"

/**************************************************************************/
/* helpers                                                                */
/**************************************************************************/

#define BSWAP  1

int main(void)
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
           nkeys, filename, st.st_size);
    printf("times: %s\n", proctimer_report(&pt, NULL));

    btree_close(&bt);

    return status;
}
