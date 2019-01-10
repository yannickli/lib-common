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

#include "arith.h"

/* This bench calculate GCD of every combination of integer of a given
 * interval using Euclid's and Stein's algorithms.
 *
 * 1/ Launch bench:
 *     $ perf record ./gcd-bench 5000 10000
 *
 * 2/ Show bench result:
 *     $ perf report
 */

int main(int argc, char **argv)
{
    int min;
    int max;

    if (argc <= 1) {
        fprintf(stderr, "usage: %s [min] max\n", argv[0]);
        return -1;
    }

    if (argc <= 2) {
        min = 1;
        max = atoi(argv[1]);

        if (max < 1) {
            fprintf(stderr, "error: max < 1 (max = %d)\n", max);
            return -1;
        }
    } else {
        min = atoi(argv[1]);
        max = atoi(argv[2]);
    }

    if (min < 1) {
        fprintf(stderr, "error: min < 1 (min = %d)\n", min);
        return -1;
    }

    if (min > max) {
        fprintf(stderr, "error: min > max (min = %d, max = %d)\n", min, max);
        return -1;
    }

    for (int i = min; i < max; i++) {
        for (int j = i; j < max; j++) {
            if (gcd_euclid(i, j) != gcd_stein(i, j)) {
                return -1;
            }
        }
    }

    return 0;
}
