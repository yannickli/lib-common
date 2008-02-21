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

#ifndef IS_LIB_COMMON_RANGE_VECTOR_H
#define IS_LIB_COMMON_RANGE_VECTOR_H

#include "array.h"

typedef struct range_elem {
    int64_t inf;
    void *data;
} range_elem;

VECTOR_TYPE(range_elem, range);
VECTOR_MEM_FUNCTIONS(range_elem, range, _vector);

/**************************************************************************/
/* Spcialized range functions                                             */
/**************************************************************************/

/* Min is inclusive,
 * Max is exclusive */
int range_vector_insert_range(range_vector *array, int64_t min, int64_t max, void *data)
    __attr_nonnull__((1));

__attr_nonnull__((1))
static inline int
range_vector_insert_one(range_vector *array, int64_t val, void *data) {
    return range_vector_insert_range(array, val, val + 1, data);
}


void *range_vector_get(const range_vector *array, int64_t val)
    __attr_nonnull__((1));

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_range_vector_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/

#endif
