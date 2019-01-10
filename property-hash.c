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

#include "property-hash.h"
#include "blob.h"

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

    s = p_dupz(buf, len);
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
    htbl_for_each_pos(i, &ph->h) {
        prop_wipe(&ph->h.tab[i]);
    }
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

void props_hash_remove(props_hash_t *ph, const char *name)
{
    props_hash_update(ph, name, NULL);
}

void props_hash_merge(props_hash_t *to, const props_hash_t *src)
{
    prop_t *pp;

    assert (to->names == src->names);
    htbl_for_each_p(pp, &src->h) {
        props_hash_update(to, pp->name, pp->value);
    }
}

/****************************************************************************/
/* Search in props_hashes                                                   */
/****************************************************************************/

const char *props_hash_findval(const props_hash_t *ph, const char *name, const char *def)
{
    uint64_t key = getkey(ph, name, false);
    prop_t  *pp = props_htbl_find(&ph->h, key);

    return (key && pp) ? pp->value : def;
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

bool props_hash_findval_bool(const props_hash_t *ph, const char *name, bool defval)
{
    const char *result;

    result = props_hash_findval(ph, name, NULL);
    if (!result)
        return defval;

    /* XXX: Copied from conf.c */
#define CONF_CHECK_BOOL(bname, value) \
    if (!strcasecmp(result, bname)) {    \
        return value;                \
    }
    CONF_CHECK_BOOL("true",  true);
    CONF_CHECK_BOOL("false", false);
    CONF_CHECK_BOOL("on",    true);
    CONF_CHECK_BOOL("off",   false);
    CONF_CHECK_BOOL("yes",   true);
    CONF_CHECK_BOOL("no",    false);
    CONF_CHECK_BOOL("1",     true);
    CONF_CHECK_BOOL("0",     false);
#undef CONF_CHECK_BOOL

    return defval;
}

/****************************************************************************/
/* Serialize props_hashes                                                   */
/****************************************************************************/

void props_hash_pack(sb_t *out, const props_hash_t *ph, int terminator)
{
    prop_t *pp;

    blob_pack(out, "d", ph->h.len);
    htbl_for_each_p(pp, &ph->h) {
        blob_pack(out, "|s|s", pp->name, pp->value);
    }
    sb_addc(out, terminator);
}

void props_hash_to_fmtv1(sb_t *out, const props_hash_t *ph)
{
    prop_t *pp;

    htbl_for_each_p(pp, &ph->h) {
        blob_pack(out, "s:s\n", pp->name, pp->value);
    }
}

void props_hash_to_conf(sb_t *out, const props_hash_t *ph)
{
    prop_t *pp;

    htbl_for_each_p(pp, &ph->h) {
        sb_addf(out, "%s = %s\n", pp->name, pp->value);
    }
}

void props_hash_to_xml(xmlpp_t *xpp, const props_hash_t *ph)
{
    prop_t *pp;

    htbl_for_each_p(pp, &ph->h) {
        xmlpp_opentag(xpp, pp->name);
        xmlpp_puts(xpp, pp->value);
        xmlpp_closetag(xpp);
    }
}

/****************************************************************************/
/* Unserialize props_hashes                                                 */
/****************************************************************************/

/* TODO check for validity first in a separate pass */
int props_hash_unpack(const void *_buf, int buflen, int *pos,
                      props_hash_t *ph, int last)
{
    const byte *buf = _buf;
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

int props_hash_from_fmtv1_data_start(props_hash_t *ph,
                                     const void *_buf, int len, int start)
{
    const char *buf = _buf;
    int pos = 0;
    sb_t key, val;

    sb_inita(&key, BUFSIZ);
    sb_inita(&val, BUFSIZ);

    if (start >= 0)
        pos = start;

    while (pos < len) {
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

        sb_set(&key, k, klen);
        if (vlen) {
            sb_set(&val, v, vlen);
            props_hash_update(ph, key.data, val.data);
        } else {
            props_hash_update(ph, key.data, NULL);
        }
        pos = end + 1 - buf;
    }

    sb_wipe(&key);
    sb_wipe(&val);
    return 0;
}

int props_hash_from_fmtv1_data(props_hash_t *ph, const void *buf, int len)
{
    return props_hash_from_fmtv1_data_start(ph, buf, len, -1);
}

int props_hash_from_fmtv1_len(props_hash_t *ph, const sb_t *payload,
                              int p_begin, int p_end)
{
    if (p_end < 0)
        return props_hash_from_fmtv1_data_start(ph, payload->data, payload->len, p_begin);
    return props_hash_from_fmtv1_data_start(ph, payload->data, p_end, p_begin);
}

int props_hash_from_fmtv1(props_hash_t *ph, const sb_t *payload)
{
    return props_hash_from_fmtv1_data_start(ph, payload->data, payload->len, -1);
}

