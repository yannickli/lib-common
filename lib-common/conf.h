/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
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
/* This module parse ini files :
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
    char **variables;
    char **values;
    int var_nb;
} conf_section_t;

typedef struct conf_t {
    conf_section_t **sections;
    int section_nb;
} conf_t;

conf_t *conf_load(const char *filename);
int conf_save(const conf_t *conf, const char *filename);

#ifdef NDEBUG
#  define conf_dump(...)
#else
void conf_dump(const conf_t *conf, int level);
#endif

const char *conf_get_raw(const conf_t *conf, const char *section, const char *var);
const char *conf_put(conf_t *conf, const char *section,
                     const char *var, const char *value);

static inline const char *
conf_get(const conf_t *conf, const char *section,
         const char *var, const char *defval)
{
    const char *res = conf_get_raw(conf, section, var);
    return res ? res : defval;
}

int conf_get_int(const conf_t *conf, const char *section,
                 const char *var, int defval);
int conf_get_bool(const conf_t *conf, const char *section,
                  const char *var, int defval);

void conf_wipe(conf_t *conf);
GENERIC_DELETE(conf_t, conf);

#endif /* IS_LIB_COMMON_CONF_H */
