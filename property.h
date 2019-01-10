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
GENERIC_INIT(property_t, property);
GENERIC_NEW(property_t, property);
GENERIC_DELETE(property_t, property);
DO_ARRAY(property_t, props, property_delete);

const char *
property_findval(const props_array *arr, const char *k, const char *def);

void props_array_qsort(props_array *arr);
void props_array_filterout(props_array *arr, const char **blacklisted);

int props_from_fmtv1_cstr(const char *buf, props_array *props);

void props_array_dup(props_array *to, const props_array *from);

#endif /* IS_LIB_COMMON_PROPERTY_H */
