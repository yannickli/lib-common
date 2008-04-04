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

#include "stats-temporal.h"

static void usage(const char *arg0)
{
    printf("usage: %s [options] filename\n"
           "\n"
           "    filename       the name of the stats file to dump\n"
           "\n"
           "Options:\n"
           "    -h             show this help\n" , arg0
           );
}
int main(int argc, char **argv)
{
    blob_t blob;
    int c;
    const char *arg0 = argv[0];

    while ((c = getopt(argc, argv, "h")) >= 0) {
        switch (c) {

          case 'h':
            usage(arg0);
            return 1;

          default:
            break;
        }
    }
    if (argc != 2) {
        usage(arg0);
        return 1;
    }
    blob_init(&blob);
    if (blob_append_file_data(&blob, argv[1]) < 0) {
        return 2;

    }
    stats_temporal_dump_auto(blob.data, blob.len);
    blob_wipe(&blob);
    return 0;
}
