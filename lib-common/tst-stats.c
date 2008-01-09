/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <stdlib.h>

#include "stats-temporal.h"
#include "err_report.h"

static int test_mode_auto(void)
{
    int status = 0;
    int desc[] = {1, 10};
    int now = 1234567890;
    blob_t output, expected;
    stats_temporal_t *stats = stats_temporal_new("/tmp/tst-stats", 1,
                                                 STATS_TEMPORAL_AUTO(1), desc);

    blob_init(&output);
    blob_init(&expected);

    e_trace(1, "Testing query: %d 10 0", now);

    stats_temporal_upd(stats, now, STATS_UPD_INCR, 0, 0, 1);
    stats_temporal_upd(stats, now + 1, STATS_UPD_INCR, 0, 0, 1);
    stats_temporal_upd(stats, now + 2, STATS_UPD_INCR, 0, 0, 2);
    stats_temporal_upd(stats, now + 3, STATS_UPD_INCR, 0, 0, 3);
    stats_temporal_upd(stats, now + 4, STATS_UPD_INCR, 0, 0, 5);
    stats_temporal_upd(stats, now + 5, STATS_UPD_INCR, 0, 0, 7);
    stats_temporal_upd(stats, now + 6, STATS_UPD_INCR, 0, 0, 11);
    stats_temporal_upd(stats, now + 7, STATS_UPD_INCR, 0, 0, 9);
    stats_temporal_upd(stats, now + 8, STATS_UPD_INCR, 0, 0, 4);
    stats_temporal_upd(stats, now + 9, STATS_UPD_INCR, 0, 0, 12);

    if (stats_temporal_query_auto(stats, &output, now, now + 10, 10,
                                  STATS_FMT_RAW)) {
        printf("Query1 failed: %d 10 0\n", now);
        status = 1;
        goto end;
    }

    blob_append_fmt(&expected, "%d %d\n", now, 1);
    blob_append_fmt(&expected, "%d %d\n",  now + 1, 1);
    blob_append_fmt(&expected, "%d %d\n",  now + 2, 2);
    blob_append_fmt(&expected, "%d %d\n",  now + 3, 3);
    blob_append_fmt(&expected, "%d %d\n",  now + 4, 5);
    blob_append_fmt(&expected, "%d %d\n",  now + 5, 7);
    blob_append_fmt(&expected, "%d %d\n",  now + 6, 11);
    blob_append_fmt(&expected, "%d %d\n",  now + 7, 9);
    blob_append_fmt(&expected, "%d %d\n",  now + 8, 4);
    blob_append_fmt(&expected, "%d %d\n",  now + 9, 12);

    if (strcmp(blob_get_cstr(&expected), blob_get_cstr(&output))) {
        printf("Query1 failed: expected:%s\nGot:\n%s",
               blob_get_cstr(&expected), blob_get_cstr(&output));
        status = 1;
        goto end;
    }

    stats_temporal_upd(stats, now + 10, STATS_UPD_INCR, 0, 0, 12);
    stats_temporal_upd(stats, now + 11, STATS_UPD_INCR, 0, 0, 7);
    stats_temporal_upd(stats, now + 12, STATS_UPD_INCR, 0, 0, 5);
    stats_temporal_upd(stats, now + 13, STATS_UPD_INCR, 0, 0, 3);
    stats_temporal_upd(stats, now + 14, STATS_UPD_INCR, 0, 0, 1);

    blob_reset(&output);
    blob_reset(&expected);

    if (stats_temporal_query_auto(stats, &output, now + 5, now + 15, 10,
                                  STATS_FMT_RAW)) {
        printf("Query2 failed: %d 10 0\n", now);
        status = 1;
        goto end;
    }

    blob_append_fmt(&expected, "%d %d\n",  now + 5, 7);
    blob_append_fmt(&expected, "%d %d\n",  now + 6, 11);
    blob_append_fmt(&expected, "%d %d\n",  now + 7, 9);
    blob_append_fmt(&expected, "%d %d\n",  now + 8, 4);
    blob_append_fmt(&expected, "%d %d\n",  now + 9, 12);
    blob_append_fmt(&expected, "%d %d\n",  now + 10, 12);
    blob_append_fmt(&expected, "%d %d\n",  now + 11, 7);
    blob_append_fmt(&expected, "%d %d\n",  now + 12, 5);
    blob_append_fmt(&expected, "%d %d\n",  now + 13, 3);
    blob_append_fmt(&expected, "%d %d\n",  now + 14, 1);

    if (strcmp(blob_get_cstr(&expected), blob_get_cstr(&output))) {
        printf("Query2 failed: expected:%s\nGot:\n%s",
               blob_get_cstr(&expected), blob_get_cstr(&output));
        status = 1;
        goto end;
    }




  end:
    blob_wipe(&output);
    blob_wipe(&expected);
    stats_temporal_delete(&stats);
    IGNORE(system("rm -f /tmp/tst-stats*"));
    return status;
}

int main(int argc, char **argv)
{
    int status = 0;
    int c;
    const char *filename = NULL;

    printf("stats-temporal tests\n");

    while ((c = getopt(argc, argv, "df:")) != -1) {
        switch (c) {
          case 'd':
            e_incr_verbosity();
            break;

          case 'f':
            filename = optarg;
            break;

          default:
            break;
        }
    }

    if (filename) {
        blob_t filedata;

        e_set_verbosity(1);
        blob_init(&filedata);
        if (blob_append_file_data(&filedata, filename) < 0) {
            e_trace(1, "Could not read %s: %m", filename);
            return 1;
        }

        stats_temporal_dump_auto(filedata.data, filedata.len);
        return 0;
    }
    status = test_mode_auto();

    if (!status) {
        printf("\nAll tests passed.\n");
    }

    return status;
}
