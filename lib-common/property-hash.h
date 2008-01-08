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

#ifndef IS_LIB_COMMON_PROPERTY_HASH_H
#define IS_LIB_COMMON_PROPERTY_HASH_H

#include "array.h"
#include "blob.h"
#include "hashtbl.h"
#include "xmlpp.h"

typedef struct props_hash_t {
    char *name;
    hashtbl_t h;
    string_hash *names;
} props_hash_t;

/****************************************************************************/
/* Create hashtables, update records                                        */
/****************************************************************************/

static inline props_hash_t *props_hash_init(props_hash_t *ph, string_hash *names)
{
    p_clear(ph, 1);
    hashtbl_init(&ph->h);
    ph->names = names;
    return ph;
}
static inline props_hash_t *props_hash_new(string_hash *names)
{
    return props_hash_init(p_new_raw(props_hash_t, 1), names);
}
props_hash_t *props_hash_dup(const props_hash_t *);
void props_hash_wipe(props_hash_t *ph);
GENERIC_DELETE(props_hash_t, props_hash);

void props_hash_update(props_hash_t *ph, const char *name, const char *value);
void props_hash_merge(props_hash_t *, const props_hash_t *);
static inline void props_hash_remove(props_hash_t *ph, const char *name)
{
    props_hash_update(ph, name, NULL);
}

/* XXX implemented in hastbl.c */
void props_hash_map(props_hash_t *,
                    void (*fn)(const char *, char **, void *), void *);

/****************************************************************************/
/* Search in props_hashes                                                   */
/****************************************************************************/

const char *props_hash_findval(const props_hash_t *ph, const char *name, const char *def);
static inline const char *props_hash_find(const props_hash_t *ph, const char *name)
{
    return props_hash_findval(ph, name, NULL);
}

int props_hash_findval_int(const props_hash_t *ph, const char *name, int defval);

/****************************************************************************/
/* Serialize props_hashes                                                   */
/****************************************************************************/

void props_hash_pack(blob_t *out, const props_hash_t *ph, int terminator);
void props_hash_to_fmtv1(blob_t *out, const props_hash_t *ph);
void props_hash_to_conf(blob_t *out, const props_hash_t *ph);
void props_hash_to_xml(xmlpp_t *pp, const props_hash_t *ph);

/****************************************************************************/
/* Unserialize props_hashes                                                 */
/****************************************************************************/

__must_check__ int props_hash_unpack(const byte *buf, int buflen, int *pos,
                                     props_hash_t *, int last);
int props_hash_from_fmtv1(props_hash_t *ph, const blob_t *payload);

#endif /* IS_LIB_COMMON_PROPERTY_H */
