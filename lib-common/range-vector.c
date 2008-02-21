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

#ifndef MINGCC
#include <alloca.h>
#endif

#include "macros.h"
#include "range-vector.h"

/*
 *  TODO:
 *
 *  - Support range collapsing: when inserting 5 -> toto into :
 *     4    5    6 ...
 *     ^    ^    ^
 *     |    |    |
 *   toto NULL titi
 *
 *   We should optimize the array to:
 *     4     6 ...
 *     ^     ^
 *     |     |
 *   toto  titi
 *
 *  - Support non-NULL last element if inf[last] == INT64_MAX
 *
 */

VECTOR_BASE_FUNCTIONS(range_elem, range, _vector);

static int range_vector_getpos(const range_vector *array, int64_t val)
{
    int l = 0, r = array->len;

    while (l < r) {
        int i = (r + l) / 2;
        switch (CMP(val, array->tab[i].inf)) {
          case CMP_LESS:
            r = i;
            break;
          case CMP_EQUAL:
            return i;
          case CMP_GREATER:
            if (i < array->len && array->tab[i + 1].inf > val)
                return i;
            l = i + 1;
            break;
        }
    }
    /* OG: why not always return l if l < array->len ? */
    return -1;
}

void *range_vector_get(const range_vector *array, int64_t val)
{
    int pos = range_vector_getpos(array, val);

    return (pos < 0) ? NULL : array->tab[pos].data;
}

#define RANGE_ELEM(min, value)  (range_elem){.inf = min, .data = value}

int range_vector_insert_range(range_vector *array, int64_t min, int64_t max, void *data)
{
    int l = 0, r = array->len;

    if (r == 0) {
        range_vector_grow(array, 2);
        array->tab[0].inf  = min;
        array->tab[0].data = data;
        array->tab[1].inf  = max;
        array->tab[1].data = NULL;
        e_trace(4, "first insert");
        return 0;
    }

    while (l < r) {
        int i = (r + l) / 2;
        switch (CMP(min, array->tab[i].inf)) {
          case CMP_LESS:
            r = i;
            break;
          case CMP_EQUAL:
            if (array->tab[i].data) {
                /* Collision */
                e_trace(4, "collision on equality of min");
                return -1;
            }
            if (i < array->len - 1) {
                switch (CMP(max, array->tab[i + 1].inf)) {
                  case CMP_LESS:
                    range_vector_insert(array, i, RANGE_ELEM(min, data));
                    range_vector_insert(array, i + 1, RANGE_ELEM(max, NULL));
                    e_trace(4, "inserted with min already present");
                    break;

                  case CMP_EQUAL:
                    /* Already the correct limit, just update the limit */
                    array->tab[i].data = data;
                    e_trace(4, "limits updated");
                    break;

                  case CMP_GREATER:
                    /* Collision */
                    e_trace(4, "collision on max > next");
                    return -1;
                }
            } else {
                array->tab[i].data = data;
                range_vector_insert(array, i + 1, RANGE_ELEM(max, NULL));
            }
            return 0;

          case CMP_GREATER:
            l = i + 1;
            break;
        }
    }

    /* Could not find a range_elem with .inf == min : we have:
     * if l > 0:       inf[l - 1] < min
     * if l < len - 1: min < inf[l + 1]
     * */
    if (max < array->tab[l].inf) {
        range_vector_insert(array, l, RANGE_ELEM(min, data));
        range_vector_insert(array, l + 1, RANGE_ELEM(max, NULL));
        e_trace(4, "inserted in between");
        return 0;
    }

    e_trace(4, "collision on max >= inf[l]");
    return -1;
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

static void range_vector_dump_str(range_vector *vec)
{
    for (int i = 0; i < vec->len; i++) {
        e_trace(1, "%lld  ->  %s", vec->tab[i].inf, (const char *)vec->tab[i].data);
    }
}

static bool range_vector_check_coherence(range_vector *vec)
{
    int64_t inf = INT64_MIN;
    void *data = NULL;

    for (int i = 0; i < vec->len; i++) {
#if 0
        /* Strict */
        if (vec->tab[i].inf <= inf || vec->tab[i].data == data) {
            return false;
        }
#else
        /* Tolerant: range vector is correct but not optimized */
        if (vec->tab[i].inf <= inf) {
            return false;
        }
#endif
        data = vec->tab[i].data;
        inf  = vec->tab[i].inf;
    }
    if (vec->len > 0 && vec->tab[vec->len - 1].data) {
        /* range vector should be NULL terminated */
        return false;
    }
    return true;
}

START_TEST(check_range_vector)
{
    const char *toto = "toto";
    const char *titi = "titi";
    const char *tutu = "tutu";
    const char *tata = "tata";
    const char *broken = "broken";
    range_vector vec;

    range_vector_init(&vec);

    //e_set_verbosity(1);

    fail_if(range_vector_insert_one(&vec, 5, (void *)toto), "Collision on %s", toto);
    fail_if(!range_vector_check_coherence(&vec), "Range vector incoherent");
    e_trace(1, "inserted 5");
    range_vector_dump_str(&vec);
    fail_if(range_vector_insert_one(&vec, 15, (void *)titi), "Collision on %s", titi);
    fail_if(!range_vector_check_coherence(&vec), "Range vector incoherent");
    e_trace(1, "inserted 15");
    range_vector_dump_str(&vec);
    fail_if(range_vector_insert_one(&vec, 8, (void *)tutu), "Collision on %s", tutu);
    fail_if(!range_vector_check_coherence(&vec), "Range vector incoherent");
    e_trace(1, "inserted 8");
    range_vector_dump_str(&vec);
    fail_if(range_vector_insert_range(&vec, 11, 14, (void *)tata), "Collision on %s", tata);
    fail_if(!range_vector_check_coherence(&vec), "Range vector incoherent");
    e_trace(1, "inserted 11 - 14");
    range_vector_dump_str(&vec);
    fail_if(range_vector_insert_range(&vec, 9, 11, (void *)toto), "Collision on %s", toto);
    fail_if(!range_vector_check_coherence(&vec), "Range vector incoherent");
    e_trace(1, "inserted 9 - 11");
    range_vector_dump_str(&vec);
    fail_if(range_vector_insert_one(&vec, 16, (void *)tutu), "Collision on %s", tutu);
    fail_if(!range_vector_check_coherence(&vec), "Range vector incoherent");
    e_trace(1, "inserted 16");
    range_vector_dump_str(&vec);
    fail_if(!range_vector_insert_range(&vec, 3, 9, (void *)broken), "Undetected collision");
    fail_if(!range_vector_check_coherence(&vec), "Range vector incoherent");
    fail_if(!range_vector_insert_range(&vec, 10, 12, (void *)broken), "Undetected collision");
    fail_if(!range_vector_check_coherence(&vec), "Range vector incoherent");
    fail_if(!range_vector_insert_range(&vec, 6, 18, (void *)broken), "Undetected collision");
    fail_if(!range_vector_check_coherence(&vec), "Range vector incoherent");

    /* Search */
    fail_if(range_vector_get(&vec, 8) != tutu, "Could not find %s", tutu);
    fail_if(range_vector_get(&vec, 10) != toto, "Could not find %s", toto);
    fail_if(range_vector_get(&vec, 8) != tutu, "Could not find %s", tutu);
    fail_if(range_vector_get(&vec, 1234), "Should not have found something");
    fail_if(range_vector_get(&vec, 1), "Should not have found something");
}
END_TEST


Suite *check_range_vector_suite(void)
{
    Suite *s  = suite_create("Range Vector");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_range_vector);
    return s;
}
#endif
