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

#ifndef IS_LIB_COMMON_PROPERTY_H
#define IS_LIB_COMMON_PROPERTY_H

#include "container.h"

typedef struct property_t {
    char *name;
    char *value;
} property_t;

static inline void property_wipe(property_t *property) {
    p_delete(&property->name);
    p_delete(&property->value);
}
GENERIC_NEW_INIT(property_t, property);
GENERIC_DELETE(property_t, property);
qvector_t(props, property_t *);

static inline void props_array_wipe(qv_t(props) *arr)
{
    qv_for_each_pos(props, pos, arr)
        property_delete(&arr->tab[pos]);
    qv_wipe(props, arr);
}
GENERIC_DELETE(qv_t(props), props_array);

const char *
property_findval(const qv_t(props) *arr, const char *k, const char *def);

void props_array_qsort(qv_t(props) *arr);
void props_array_filterout(qv_t(props) *arr, const char **blacklisted);

int props_from_fmtv1_cstr(const char *buf, qv_t(props) *props);

void props_array_dup(qv_t(props) *to, const qv_t(props) *from);

#endif /* IS_LIB_COMMON_PROPERTY_H */
