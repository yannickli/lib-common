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

#include "property-hash.h"
#include "string_is.h"

/****************************************************************************/
/* Generic helpers and functions                                            */
/****************************************************************************/
/* XXX: subtle
 *
 * ->names is a uniquifyer: once a string entered it, it never goes out.
 *   names are lowercased to guarantee those are unique.
 *
 * ->h is a htbl whose keys are the addresses of the unique strings in
 * ->names, and whose value is the actual value.
 *
 * NOTE: This code makes the breathtaking assumption that uint64_t's are
 *       enough to store a pointer, it's enforced in getkey
 */


static uint64_t getkey(const props_hash_t *ph, const char *name, bool insert)
{
    char buf[BUFSIZ], *s, **sp;
    int len = 0;

    STATIC_ASSERT(sizeof(uint64_t) >= sizeof(uintptr_t));

    for (const char *p = name; *p && len < ssizeof(buf) - 1; p++, len++) {
        buf[len] = tolower((unsigned char)*p);
    }
    buf[len] = '\0';

    sp = string_htbl_find(ph->names, buf, len);
    if (sp)
        return (uintptr_t)*sp;
    if (!insert)
        return 0; /* NULL */

    s = p_dupstr(buf, len);
    string_htbl_insert(ph->names, s, len);
    return (uintptr_t)s;
}

/****************************************************************************/
/* Create hashtables, update records                                        */
/****************************************************************************/

props_hash_t *props_hash_dup(const props_hash_t *ph)
{
    props_hash_t *res = props_hash_new(ph->names);
    if (ph->name)
        res->name = p_strdup(ph->name);
    props_hash_merge(res, ph);
    return res;
}

void props_hash_wipe(props_hash_t *ph)
{
    HTBL_MAP(&ph->h, prop_wipe);
    props_htbl_wipe(&ph->h);
    p_delete(&ph->name);
}

void props_hash_update(props_hash_t *ph, const char *name, const char *value)
{
    uint64_t key = getkey(ph, name, true);
    prop_t *pp;

    if (value) {
        char *v = p_strdup(value);
        prop_t prop = { .value = v };
        prop.key = key;
        pp = props_htbl_insert(&ph->h, prop);
        if (pp) {
            SWAP(char *, pp->value, v);
            p_delete(&v);
        }
    } else {
        pp = props_htbl_find(&ph->h, key);
        if (pp) {
            p_delete(&pp->value);
            props_htbl_ll_remove(&ph->h, pp);
        }
    }
}

static void update_one(prop_t *p, void *to)
{
    props_hash_update(to, p->key_s, p->value);
}

void props_hash_merge(props_hash_t *to, const props_hash_t *src)
{
    assert (to->names == src->names);
    HTBL_MAP((props_htbl *)&src->h, update_one, to);
}

/****************************************************************************/
/* Search in props_hashes                                                   */
/****************************************************************************/

const char *props_hash_findval(const props_hash_t *ph, const char *name, const char *def)
{
    uint64_t key = getkey(ph, name, false);
    prop_t  *pp  = props_htbl_find(&ph->h, key);
    return key && pp ? pp->value : def;
}

int props_hash_findval_int(const props_hash_t *ph, const char *name, int defval)
{
    const char *result;

    result = props_hash_findval(ph, name, NULL);
    if (result) {
        return atoi(result);
    } else {
        return defval;
    }
}

/****************************************************************************/
/* Serialize props_hashes                                                   */
/****************************************************************************/

static void pack_one(prop_t *pp, void *blob)
{
    blob_pack(blob, "|s|s", pp->key_s, pp->value);
}

void props_hash_pack(blob_t *out, const props_hash_t *ph, int terminator)
{
    blob_pack(out, "d", ph->h.len);
    HTBL_MAP((props_htbl *)&ph->h, pack_one, out);
    blob_append_byte(out, terminator);
}

static void one_to_fmtv1(prop_t *pp, void *blob)
{
    blob_pack(blob, "s:s\n", pp->key_s, pp->value);
}

void props_hash_to_fmtv1(blob_t *out, const props_hash_t *ph)
{
    HTBL_MAP((props_htbl *)&ph->h, one_to_fmtv1, out);
}

static void one_to_conf(prop_t *pp, void *blob)
{
    /* fixme val could have embeded \n */
    blob_append_fmt(blob, "%s = %s\n", pp->key_s, pp->value);
}

void props_hash_to_conf(blob_t *out, const props_hash_t *ph)
{
    HTBL_MAP((props_htbl *)&ph->h, one_to_conf, out);
}

static void one_to_xml(prop_t *pp, void *xpp)
{
    xmlpp_opentag(xpp, pp->key_s);
    xmlpp_puttext(xpp, pp->value, -1);
    xmlpp_closetag(xpp);
}

void props_hash_to_xml(xmlpp_t *pp, const props_hash_t *ph)
{
    HTBL_MAP((props_htbl *)&ph->h, one_to_xml, pp);
}

/****************************************************************************/
/* Unserialize props_hashes                                                 */
/****************************************************************************/

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

    while (len-- > 0) {
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

    blob_inita(&key, BUFSIZ);
    blob_inita(&val, BUFSIZ);
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
        while (vlen > 0 && isspace((unsigned char)v[vlen - 1]))
            vlen--;

        blob_set_data(&key, k, klen);
        if (vlen) {
            blob_set_data(&val, v, vlen);
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

static void prop_hash_map_one(prop_t *pp, props_htbl *t,
                              void (*fn)(const char *, char **, void *),
                              void *priv)
{
    (*fn)(pp->key_s, &pp->value, priv);
    if (!pp->value) {
        props_htbl_ll_remove(t, pp);
    }
}

void props_hash_map(props_hash_t *ph,
                    void (*fn)(const char *, char **, void *), void *priv)
{
    HTBL_MAP(&ph->h, prop_hash_map_one, &ph->h, fn, priv);
    if (4 * p_alloc_nr(ph->h.len) < ph->h.size)
        props_htbl_resize(&ph->h, 2 * p_alloc_nr(ph->h.len));
}
