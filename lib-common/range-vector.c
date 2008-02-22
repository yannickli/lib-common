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
            if (i + 1 < array->len && array->tab[i + 1].inf > val)
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

/* OG: Should have an API to reset a range and another one to add a
 * range with collision control.
 */
int range_vector_insert_range(range_vector *array,
                              int64_t min, int64_t max, void *data)
{
    int i, l = 0, r = array->len;

    while (l < r) {
        i = (r + l) >> 1;
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
	    if (i == array->len - 1 || max < array->tab[i + 1].inf) {
		array->tab[i].data = data;
                range_vector_insert(array, i + 1, RANGE_ELEM(max, NULL));
		e_trace(4, "inserted with min already present");
	    } else
	    if (max == array->tab[i + 1].inf) {
		/* Already the correct limit, just update the limit */
		array->tab[i].data = data;
		e_trace(4, "limits updated");
		if (array->tab[i + 1].data == data) {
		    /* Merge with next cell */
		    range_vector_remove(array, i + 1);
		    e_trace(4, "merge with next cell");
		}
	    } else {
		/* Collision */
		e_trace(4, "collision on max > next");
		return -1;
            }
	    if (i > 0 && array->tab[i - 1].data == data) {
		/* Merge with previous cell */
		range_vector_remove(array, i);
		e_trace(4, "merge with previous cell");
	    }
	    return 0;

	  case CMP_GREATER:
	      l = i + 1;
	      break;
	}
    }
    i = l;

    if (i > 0 && array->tab[i - 1].data) {
	/* Collision */
	e_trace(4, "collision on split");
	return -1;
    }

    if (i == array->len || max < array->tab[i].inf) {
	range_vector_insert(array, i, RANGE_ELEM(min, data));
	range_vector_insert(array, i + 1, RANGE_ELEM(max, NULL));
	e_trace(4, "split in 3 parts on insertion");
    } else
    if (max == array->tab[i].inf) {
	if (array->tab[i].data == data) {
	    array->tab[i].inf = min;
	    e_trace(4, "extend existing range downwards");
	} else {
	    range_vector_insert(array, i, RANGE_ELEM(min, data));
	    e_trace(4, "split in 2 parts on insertion");
	}
    } else {
	/* Collision */
	e_trace(4, "collision on split");
	return -1;
    }
    return 0;
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

static void range_vector_dump_str(range_vector *vec)
{
    for (int i = 0; i < vec->len; i++) {
        e_trace(1, "%lld  ->  %s", (long long)vec->tab[i].inf,
                (const char *)vec->tab[i].data);
    }
}

static bool range_vector_check_coherence(range_vector *vec, bool strict)
{
    int64_t inf;
    void *data;

    if (vec->len < 0)
        return false;

    if (vec->len == 0)
        return true;

    data = vec->tab[0].data;
    inf  = vec->tab[0].inf;
    for (int i = 1; i < vec->len; i++) {
        if (vec->tab[i].inf <= inf)
            return false;

        /* Tolerant: range vector is correct but not optimized */
        if (strict && vec->tab[i].data == data)
            return false;

        data = vec->tab[i].data;
        inf  = vec->tab[i].inf;
    }
    if (data) {
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

#define TEST_ONE(num, data)  do {                                         \
        e_trace(1, "inserting %d", num);                                  \
        fail_if(range_vector_insert_one(&vec, num, (void *)(data)),       \
                "Collision on %s", data);                                 \
        range_vector_dump_str(&vec);                                      \
        fail_if(!range_vector_check_coherence(&vec, true),                \
                "Range vector incoherent");                               \
    } while (0)

#define TEST_RANGE(min, max, data)  do {                                  \
        e_trace(1, "inserted %d ... %d", min, max);                       \
        fail_if(range_vector_insert_range(&vec, min, max, (void*)(data)), \
                "Collision on %s", data);                                 \
        range_vector_dump_str(&vec);                                      \
        fail_if(!range_vector_check_coherence(&vec, true),                \
                "Range vector incoherent");                               \
    } while (0)

    TEST_ONE(3, toto);
    TEST_ONE(1, tata);
    TEST_ONE(0, toto);
    TEST_ONE(15, titi);
    TEST_ONE(4, tutu);
    TEST_ONE(8, tutu);
    TEST_RANGE(11, 14, tata);
    TEST_RANGE(9, 11, toto);
    TEST_ONE(16, tutu);

    fail_if(!range_vector_insert_range(&vec, 3, 9, (void *)broken), "Undetected collision");
    fail_if(!range_vector_check_coherence(&vec, true),
            "Range vector incoherent");
    fail_if(!range_vector_insert_range(&vec, 10, 12, (void *)broken), "Undetected collision");
    fail_if(!range_vector_check_coherence(&vec, true),
            "Range vector incoherent");
    fail_if(!range_vector_insert_range(&vec, 6, 18, (void *)broken), "Undetected collision");
    fail_if(!range_vector_check_coherence(&vec, true),
            "Range vector incoherent");

    /* Search */
    fail_if(range_vector_get(&vec, 8) != tutu, "Could not find %s", tutu);
    fail_if(range_vector_get(&vec, 10) != toto, "Could not find %s", toto);
    fail_if(range_vector_get(&vec, 8) != tutu, "Could not find %s", tutu);
    fail_if(range_vector_get(&vec, 1234), "Should not have found something");
    fail_if(range_vector_get(&vec, -1), "Should not have found something");
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
