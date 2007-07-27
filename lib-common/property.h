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

#ifndef IS_LIB_COMMON_PROPERTY_H
#define IS_LIB_COMMON_PROPERTY_H

#include "array.h"
#include "blob.h"

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
DO_ARRAY(property_t, property, property_delete);

void property_array_update(property_array *arr,
                           const char *k, const char *v);

property_t *property_find(const property_array *arr, const char *k);
const char *
property_findval(const property_array *arr, const char *k, const char *def);
void property_array_merge(property_array *arr, property_array **old);

void property_array_remove_nulls(property_array *arr);

/* appends $nb|$k1|$v1|...|$kn|$vn$last */
void property_array_pack(blob_t *out, const property_array *arr, int last);

int property_array_unpack(const byte *buf, int buflen,  int *pos,
                          property_array **arr, int last)
    __must_check__;
int props_from_fmtv1(const blob_t *payload, property_array *props);
void props_to_fmtv1(blob_t *out, property_array *props);

void property_array_dup(const property_array *from, property_array *to);
#endif /* IS_LIB_COMMON_PROPERTY_H */
