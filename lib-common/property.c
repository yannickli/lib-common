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

#include "property.h"
#include "string_is.h"

void props_array_update(props_array *arr, const char *k, const char *v)
{
    int i;

    for (i = 0; i < arr->len; i++) {
        if (!strcasecmp(arr->tab[i]->name, k)) {
            if (v) {
                /* Update property value */
                p_delete(&arr->tab[i]->value);
                arr->tab[i]->value = p_strdup(v);
            } else {
                /* value == NULL -> delete property */
                property_t *prop = props_array_take(arr, i);
                property_delete(&prop);
            }
            return;
        }
    }

    if (v) {
        /* Property not found, create it if needed */
        property_t *prop = property_new();
        prop->name  = p_strdup(k);
        prop->value = p_strdup(v);
        props_array_append(arr, prop);
    }
}

property_t *property_find(const props_array *arr, const char *k)
{
    int i;

    for (i = 0; i < arr->len; i++) {
        property_t *prop = arr->tab[i];
        if (!strcasecmp(k, prop->name)) {
            return prop;
        }
    }

    return NULL;
}

const char *
property_findval(const props_array *arr, const char *k, const char *def)
{
    int i;

    for (i = 0; i < arr->len; i++) {
        property_t *prop = arr->tab[i];
        if (!strcasecmp(k, prop->name)) {
            return prop->value ? prop->value : def;
        }
    }

    return def;
}

void props_array_merge(props_array *arr, props_array **old)
{
    int i, j;

    /* find every property of arr that is in old, and merge them */
    for (i = 0; i < arr->len; i++) {
        property_t *prop = arr->tab[i];

        for (j = 0; j < (*old)->len; j++) {
            if (!strcasecmp(prop->name, (*old)->tab[j]->name)) {
                property_t *oldprop = props_array_take(*old, j);

                if (oldprop->value) {
                    SWAP(prop->value, oldprop->value);
                } else {
                    prop = props_array_take(arr, i);
                    property_delete(&prop);
                }

                property_delete(&oldprop);
                break;
            }
        }
    }

    /* append the rest of old */
    for (i = 0; i < (*old)->len; i++) {
        if ((*old)->tab[i]->value) {
            props_array_append(arr, (*old)->tab[i]);
            (*old)->tab[i] = NULL;
        } else {
            property_delete(&(*old)->tab[i]);
        }
    }
    props_array_reset(*old);
    props_array_delete(old);
}

void props_array_remove_nulls(props_array *arr)
{
    int i = arr->len;

    while (i-- > 0) {
        if (!arr->tab[i]->value) {
            property_t *prop = props_array_take(arr, i);
            property_delete(&prop);
        }
    }
}

void props_array_pack(blob_t *out, const props_array *arr, int last)
{
    int i, nulls = 0;

    for (i = 0; i < arr->len; i++) {
        nulls += !arr->tab[i]->value;
    }
    blob_pack(out, "d", arr->len - nulls);
    for (i = 0; i < arr->len; i++) {
        if (arr->tab[i]->value) {
            blob_pack(out, "|s|s", arr->tab[i]->name, arr->tab[i]->value);
        }
    }

    if (last) {
        blob_append_byte(out, last);
    }
}

int props_array_unpack(const byte *buf, int buflen, int *pos,
                          props_array **arrout, int last)
{
    int len, pos0 = *pos;
    props_array *arr = props_array_new();

    if (buf_unpack(buf, buflen, pos, "d|", &len) < 1) {
        char fmt[3] = {'d', last, '\0' };
        if (buf_unpack(buf, buflen, pos, fmt, &len) < 1 || len != 0) {
            goto error;
        }
    }

    while (len-- > 0) {
        static char fmt[5] = {'s', '|', 's', '|', '\0'};
        property_t *prop = property_new();
        int res;

        fmt[3] = len ? '|' : last;
        res = buf_unpack(buf, buflen, pos, fmt, &prop->name, &prop->value);
        props_array_append(arr, prop);
        if (res < 2)
            goto error;
    }

    *arrout = arr;
    return 1;

  error:
    *pos = pos0;
    props_array_delete(&arr);
    return 0;
}

int props_from_fmtv1(const blob_t *payload, props_array *props)
{
    const char *buf = blob_get_cstr(payload);
    int pos = 0;

    while (pos < payload->len) {
        const char *k, *v, *end;
        int klen, vlen;
        property_t *prop;

        k    = skipblanks(buf + pos);
        klen = strcspn(k, " \t:");

        v    = skipblanks(k + klen);
        if (*v != ':')
            return -1;
        v    = skipblanks(v + 1);
        end  = strchr(v, '\n');
        if (!end)
            return -1;
        vlen = end - v;
        while (vlen > 0 && isspace((unsigned char)v[vlen - 1]))
            vlen--;

        prop = property_new();
        prop->name  = p_dupstr(k, klen);
        prop->value = p_dupstr(v, vlen);
#if 0   // XXX: NULL triggers Segfault :(
        prop->value = vlen ? p_dupstr(v, vlen) : NULL;
#endif
        props_array_append(props, prop);

        pos = end + 1 - buf;
    }

    return 0;
}

void props_to_fmtv1(blob_t *out, const props_array *props)
{
    int i;

    for (i = 0; i < props->len; i++) {
        blob_pack(out, "s:s\n",props->tab[i]->name, props->tab[i]->value);
    }
}

void props_array_dup(props_array *to, const props_array *from)
{
    int i;

    for (i = 0; i < from->len; i++) {
        property_t *prop = property_new();

        prop->name  = p_strdup(from->tab[i]->name);
        prop->value = p_strdup(from->tab[i]->value);
        props_array_append(to, prop);
    }
}

#ifndef NDEBUG
void props_array_dump(int level, const props_array *props)
{
    int i;

    for (i = 0; i < props->len; i++) {
        e_trace(level, "%s: %s", props->tab[i]->name, props->tab[i]->value);
    }
}
#endif
