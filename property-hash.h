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

#ifndef IS_LIB_COMMON_PROPERTY_HASH_H
#define IS_LIB_COMMON_PROPERTY_HASH_H

#include "container.h"
#include "xmlpp.h"

typedef struct prop_t {
    union {
        uintptr_t key;
        const char *name;
    };
    char *value;
} prop_t;

static inline void prop_wipe(prop_t *p) {
    p_delete(&p->value);
}

DO_HTBL_KEY(prop_t, uintptr_t, props, key);

typedef struct props_hash_t {
    char *name;
    props_htbl h;
    string_htbl *names;
} props_hash_t;

/****************************************************************************/
/* Create hashtables, update records                                        */
/****************************************************************************/

static inline props_hash_t *props_hash_init(props_hash_t *ph, string_htbl *names)
{
    p_clear(ph, 1);
    props_htbl_init(&ph->h);
    ph->names = names;
    return ph;
}
static inline props_hash_t *props_hash_new(string_htbl *names)
{
    return props_hash_init(p_new_raw(props_hash_t, 1), names);
}
props_hash_t *props_hash_dup(const props_hash_t *);
void props_hash_wipe(props_hash_t *ph);
GENERIC_DELETE(props_hash_t, props_hash);

void props_hash_update(props_hash_t *ph, const char *name, const char *value);
void props_hash_remove(props_hash_t *ph, const char *name);
void props_hash_merge(props_hash_t *, const props_hash_t *);

#define PROPS_HASH_MAP(ph, f, ...)   HTBL_MAP(&(ph)->h, f, ##__VA_ARGS__)

/****************************************************************************/
/* Search in props_hashes                                                   */
/****************************************************************************/

const char *props_hash_findval(const props_hash_t *ph, const char *name, const char *def);
static inline const char *props_hash_find(const props_hash_t *ph, const char *name)
{
    return props_hash_findval(ph, name, NULL);
}

int props_hash_findval_int(const props_hash_t *ph, const char *name, int defval);
bool props_hash_findval_bool(const props_hash_t *ph, const char *name, bool defval);

/****************************************************************************/
/* Serialize props_hashes                                                   */
/****************************************************************************/

void props_hash_pack(sb_t *out, const props_hash_t *ph, int terminator);
void props_hash_to_fmtv1(sb_t *out, const props_hash_t *ph);
void props_hash_to_conf(sb_t *out, const props_hash_t *ph);
void props_hash_to_xml(xmlpp_t *pp, const props_hash_t *ph);

/****************************************************************************/
/* Unserialize props_hashes                                                 */
/****************************************************************************/

__must_check__ int props_hash_unpack(const void *buf, int buflen, int *pos,
                                     props_hash_t *, int last);
int props_hash_from_fmtv1_data_start(props_hash_t *ph, const void *data,
                                     int len, int start);
int props_hash_from_fmtv1_data(props_hash_t *ph, const void *data, int len);
int props_hash_from_fmtv1(props_hash_t *ph, const sb_t *payload);
int props_hash_from_fmtv1_len(props_hash_t *ph, const sb_t *payload,
                              int begin, int end);

#endif /* IS_LIB_COMMON_PROPERTY_H */
