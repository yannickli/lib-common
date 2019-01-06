/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_SORT_H
# error "you must include sort.h instead"
#endif

static inline size_t (bisect)(type_t what, const type_t data[], size_t len,
                              bool *found)
{
    size_t l = 0, r = len;

    while (l < r) {
        size_t i = (l + r) / 2;

        if (what == data[i]) {
            if (found) {
                *found = true;
            }
            return i;
        }
        if (what < data[i]) {
            r = i;
        } else {
            l = i + 1;
        }
    }
    if (found) {
        *found = false;
    }
    return r;
}

static inline bool (contains)(type_t what, const type_t data[], size_t len)
{
    size_t l = 0, r = len;

    while (l < r) {
        size_t i = (l + r) / 2;

        if (what == data[i])
            return true;
        if (what < data[i]) {
            r = i;
        } else {
            l = i + 1;
        }
    }
    return false;
}

#undef type_t
#undef bisect
#undef contains
