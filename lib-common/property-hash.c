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

#include "property-hash.h"

static void props_wipe_one(void **s, void *unused)
{
    p_delete(s);
}

void props_hash_wipe(props_hash_t *ph)
{
    hashtbl_map(&ph->h, &props_wipe_one, NULL);
    hashtbl_wipe(&ph->h);
}

/****************************************************************************/
/* deal with our curious hashtables                                         */
/****************************************************************************/
/* XXX: subtle
 *
 * ->names is a uniquifyer: once a string entered it, it never goes out.
 *   names are lowercased to guarantee those are unique.
 *
 * ->h is a hashtbl_t whose keys are the addresses of the unique strings in
 * ->names, and whose value is the actual value.
 *
 * NOTE: This code makes the breathtaking assumption that uint64_t's are
 *       enough to store a pointer, it's enforced in getkey
 */


static uint64_t getkey(props_hash_t *ph, const char *name, bool insert)
{
    char buf[BUFSIZ], *s, **sp;
    int len = 0;
    uint64_t key;

    STATIC_ASSERT(sizeof(uint64_t) >= sizeof(uintptr_t));

    for (const char *p = name; *p && len < ssizeof(buf) - 1; p++, len++) {
        buf[len] = tolower(*p);
    }
    buf[len] = '\0';

    key = combined_hash((const byte *)buf, len);
    sp  = string_hash_find(ph->names, key, buf, len);
    if (sp)
        return (uintptr_t)*sp;
    if (!insert)
        return 0; /* NULL */

    s = p_dupstr(name, len);
    string_hash_insert(ph->names, key, s);
    return (uintptr_t)s;
}

static const char *getname(uint64_t key)
{
    return (const char *)(uintptr_t)key;
}

void props_hash_update(props_hash_t *ph, const char *name, const char *value)
{
    uint64_t key = getkey(ph, name, true);

    if (value && *value) {
        char *v   = strdup(value);
        char **sp = (char **)hashtbl_insert(&ph->h, key, v);

        if (sp) {
            SWAP(*sp, v);
            p_delete(&v);
        }
    } else {
        hashtbl_remove(&ph->h, hashtbl_find(&ph->h, key));
    }
}

const char *props_hash_findval(props_hash_t *ph, const char *name, const char *def)
{
    uint64_t key = getkey(ph, name, false);
    char **sp    = (char **)hashtbl_find(&ph->h, key);
    return key && sp ? *sp : def;
}

static void pack_one(uint64_t key, void **val, void *blob)
{
    blob_pack(blob, "|s|s", getname(key), *val);
}

void props_hash_pack(blob_t *out, const props_hash_t *ph, int terminator)
{
    blob_pack(out, "d", ph->h.nr);
    hashtbl_map2((hashtbl_t *)&ph->h, &pack_one, out);
    blob_append_byte(out, terminator);
}

static void one_to_fmtv1(uint64_t key, void **val, void *blob)
{
    blob_pack(blob, "s:s\n", getname(key), *val);
}

void props_hash_to_fmtv1(blob_t *out, const props_hash_t *ph)
{
    hashtbl_map2((hashtbl_t *)&ph->h, &one_to_fmtv1, out);
}

/* TODO check for validity first in a separate pass */
int props_hash_unpack(const byte *buf, int buflen, int *pos,
                      props_hash_t *ph, int last)
{
    int len, pos0 = *pos;

    if (buf_unpack(buf, buflen, pos, "d|", &len) < 1) {
        char fmt[3] = {'d', last, '\0' };
        if (buf_unpack(buf, buflen, pos, fmt, &len) < 1 || len != 0) {
            goto error;
        }
    }

    for (int i = 0; i < len; i++) {
        static char fmt[5] = {'s', '|', 's', '|', '\0'};
        char *key, *val;
        int res;

        fmt[3] = len ? '|' : last;
        res = buf_unpack(buf, buflen, pos, fmt, &key, &val);
        if (res < 2) {
            p_delete(&key);
            goto error;
        }
        props_hash_update(ph, key, val);
        p_delete(&val);
        p_delete(&key);
    }
    return 1;

  error:
    *pos = pos0;
    return 0;
}

int props_hash_from_fmtv1(props_hash_t *ph, const blob_t *payload)
{
    const char *buf = blob_get_cstr(payload);
    int pos = 0;
    blob_t key, val;
    blob_init(&key);
    blob_init(&val);

    while (pos < payload->len) {
        const char *k, *v, *end;
        int klen, vlen;

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
        while (vlen > 0 && isspace(v[vlen - 1]))
            vlen--;

        blob_set_data(&key, k, klen);
        if (vlen) {
            blob_set_data(&key, v, vlen);
            props_hash_update(ph, blob_get_cstr(&key), blob_get_cstr(&val));
        } else {
            props_hash_update(ph, blob_get_cstr(&key), NULL);
        }
        pos = end + 1 - buf;
    }

    blob_wipe(&key);
    blob_wipe(&val);
    return 0;
}
