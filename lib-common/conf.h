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

#ifndef IS_LIB_COMMON_CONF_H
#define IS_LIB_COMMON_CONF_H

#include "mem.h"
#include "blob.h"
#include "property.h"

/* This module parses ini files :
 *
 * [Section1]
 *
 * var1 = value1
 * ; Comments
 * var2 = value2
 *
 * [Section2]
 *
 * # Comments
 * var3 = value3
 */

typedef struct conf_section_t {
    char *name;
    property_array vals;
} conf_section_t;

ARRAY_TYPE(conf_section_t, conf_section);

typedef struct conf_t {
    conf_section_array sections;
} conf_t;

void conf_wipe(conf_t *conf);
GENERIC_DELETE(conf_t, conf);

conf_t *conf_load(const char *filename);
/* Same as conf_load, but from a blob.
 * ACHTUNG MINEN : the blob is modified !!!
 * */
conf_t *conf_load_blob(blob_t *buf);
int conf_save(const conf_t *conf, const char *filename);

static inline const conf_section_t *
conf_get_section(const conf_t *conf, int i)
{
    return conf->sections.tab[i];
}
const char *conf_get_raw(const conf_t *conf,
                         const char *section, const char *var);
const char *
conf_section_get_raw(const conf_section_t *section,
                     const char *var);

static inline const char *
conf_get(const conf_t *conf, const char *section,
         const char *var, const char *defval)
{
    const char *res = conf_get_raw(conf, section, var);
    return res ? res : defval;
}

static inline const char *
conf_section_get(const conf_section_t *section,
         const char *var, const char *defval)
{
    const char *res = conf_section_get_raw(section, var);
    return res ? res : defval;
}



const char *conf_put(conf_t *conf, const char *section,
                     const char *var, const char *value);

int conf_get_int(const conf_t *conf, const char *section,
                 const char *var, int defval);
int conf_get_bool(const conf_t *conf, const char *section,
                  const char *var, int defval);
int conf_section_get_int(const conf_section_t *section,
                         const char *var, int defval);
int conf_section_get_bool(const conf_section_t *section,
                          const char *var, int defval);

/* Lookup next section beginning with prefix.
 * Store section name remaining characters in suffix if not NULL */
int conf_next_section_idx(const conf_t *conf, const char *prefix,
                          int prev_idx, const char **suffixp);

#ifdef NDEBUG
#  define conf_dump(...)
#else
static inline void conf_dump(const conf_t *conf, int level)
{
    if (e_is_traced((level))) {
        conf_save((conf), "/dev/stderr");
    }
}
#endif

#ifdef CHECK
#include <check.h>

Suite *check_conf_suite(void);

#endif

#endif /* IS_LIB_COMMON_CONF_H */
