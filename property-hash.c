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
    char buf[BUFSIZ], *s;
    int len = 0, pos;
    uint32_t h;

    STATIC_ASSERT(sizeof(uint64_t) >= sizeof(uintptr_t));

    for (const char *p = name; *p && len < ssizeof(buf) - 1; p++, len++) {
        buf[len] = tolower((unsigned char)*p);
    }
    buf[len] = '\0';

    h   = jenkins_hash(buf, len);
    pos = qh_find_h(str, ph->names, h, buf);
    if (pos >= 0)
        return (uintptr_t)ph->names->keys[pos];
    if (!insert)
        return 0; /* NULL */
    s = p_dupz(buf, len);
    qh_add_h(str, ph->names, h, s);
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
    qm_for_each_pos(proph, i, &ph->h) {
        p_delete(&ph->h.values[i]);
    }
    qm_wipe(proph, &ph->h);
    p_delete(&ph->name);
}

static void props_hash_update_key(props_hash_t *ph, uint64_t key, const char *value)
{
    if (value) {
        char *v = p_strdup(value);
        int pos = __qm_put(proph, &ph->h, key, v, 0);

        if (pos & QHASH_COLLISION) {
            pos ^= QHASH_COLLISION;
            p_delete(&ph->h.values[pos]);
            ph->h.values[pos] = v;
        }
    } else {
        int pos = qm_del_key(proph, &ph->h, key);

        if (pos >= 0)
            p_delete(&ph->h.values[pos]);
    }
}

void props_hash_update(props_hash_t *ph, const char *name, const char *value)
{
    props_hash_update_key(ph, getkey(ph, name, true), value);
}

void props_hash_remove(props_hash_t *ph, const char *name)
{
    props_hash_update(ph, name, NULL);
}

void props_hash_merge(props_hash_t *to, const props_hash_t *src)
{
    assert (to->names == src->names);
    qm_for_each_pos(proph, pos, &src->h) {
        props_hash_update_key(to, src->h.keys[pos], src->h.values[pos]);
    }
}

/****************************************************************************/
/* Search in props_hashes                                                   */
/****************************************************************************/

const char *props_hash_findval(const props_hash_t *ph, const char *name, const char *def)
{
    uint64_t key = getkey(ph, name, false);
    int pos = qm_find_safe(proph, &ph->h, key);

    return pos < 0 ? def : ph->h.values[pos];
}

int props_hash_findval_int(const props_hash_t *ph, const char *name, int defval)
{
    const char *result = props_hash_findval(ph, name, NULL);

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
    blob_pack(out, "d", qm_len(proph, &ph->h));
    qm_for_each_pos(proph, pos, &ph->h) {
        blob_pack(out, "|s|s", (char *)(uintptr_t)ph->h.keys[pos],
                  ph->h.values[pos]);
    }
    sb_addc(out, terminator);
}

void props_hash_to_fmtv1(sb_t *out, const props_hash_t *ph)
{
    qm_for_each_pos(proph, pos, &ph->h) {
        blob_pack(out, "s:s\n", (char *)(uintptr_t)ph->h.keys[pos],
                  ph->h.values[pos]);
    }
}

void props_hash_to_conf(sb_t *out, const props_hash_t *ph)
{
    qm_for_each_pos(proph, pos, &ph->h) {
        sb_addf(out, "%s = %s\n", (char *)(uintptr_t)ph->h.keys[pos],
                ph->h.values[pos]);
    }
}

void props_hash_to_xml(xmlpp_t *xpp, const props_hash_t *ph)
{
    qm_for_each_pos(proph, pos, &ph->h) {
        xmlpp_opentag(xpp, (char *)(uintptr_t)ph->h.keys[pos]);
        xmlpp_puts(xpp, ph->h.values[pos]);
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

