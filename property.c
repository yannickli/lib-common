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

#include "property.h"

const char *
property_findval(const qv_t(props) *arr, const char *k, const char *def)
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

/* OG: should take buf+len with len<0 for strlen */
int props_from_fmtv1_cstr(const char *buf, qv_t(props) *props)
{
    int pos = 0;
    int len = strlen(buf);

    while (pos < len) {
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
        prop->name  = p_dupz(k, klen);
        prop->value = p_dupz(v, vlen);
#if 0   // XXX: NULL triggers Segfaults in user code :(
        prop->value = vlen ? p_dupz(v, vlen) : NULL;
#endif
        qv_append(props, props, prop);

        pos = end + 1 - buf;
    }

    return 0;
}
