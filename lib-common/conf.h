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

#ifndef IS_LIB_COMMON_CONF_H
#define IS_LIB_COMMON_CONF_H

/* {{{ cfg files are ini-like files with an extended format.

  - leading and trailing spaces aren't significant.
  - any line can be wrapped with a backslash (`\`) as a continuation token.
  - quoted strings can embed usual C escapes (\a \b \n ...), octal chars
    (\ooo) and hexadecimal ones (\x??) and unicode ones (\u????).

  Encoding should be utf-8.

----8<----

[simple]
key = value

[section "With a Name"]
key = 1234
other = "some string with embeded spaces"
; comment
# alternate comment form
foo = /some/value/without[spaces|semicolon|dash]

[section "With a very very way too long Name to show the \
line splitting feature, but beware, spaces after a continuation are \
significant"]


---->8----
}}} */

#include "blob.h"
#include "string_is.h"
#include "property.h"

/****************************************************************************/
/* Low level API                                                            */
/****************************************************************************/

enum cfg_parse_opts {
    CFG_PARSE_OLD_NAMESPACES = 1,
    CFG_PARSE_OLD_KEYS       = 2,
};

typedef enum cfg_parse_evt {
    CFG_PARSE_SECTION,     /* v isn't NULL and vlen is >= 1 */
    CFG_PARSE_SECTION_ID,  /* v isn't NULL and vlen is >= 1 */
    CFG_PARSE_KEY,         /* v isn't NULL and vlen is >= 1 */

    CFG_PARSE_VALUE,       /* v may be NULL                 */
    CFG_PARSE_EOF,         /* v is NULL                     */

    CFG_PARSE_ERROR,       /* v isn't NULL and vlen is >= 1 */
} cfg_parse_evt;

typedef int cfg_parse_hook(void *priv, cfg_parse_evt,
                           const char *, int len, void *ctx);

int cfg_parse(const char *file, cfg_parse_hook *, void *, int opts);
int cfg_parse_buf(const char *, ssize_t, cfg_parse_hook *, void *, int opts);

__attr_printf__(3, 0)
int cfg_parse_seterr(void *ctx, int offs, const char *fmt, ...);


/****************************************************************************/
/* conf_t's                                                                 */
/****************************************************************************/

typedef struct conf_section_t {
    char *name;
    props_array vals;
} conf_section_t;

ARRAY_TYPE(conf_section_t, conf_section);

typedef struct conf_t {
    conf_section_array sections;
} conf_t;

void conf_wipe(conf_t *conf);
GENERIC_DELETE(conf_t, conf);

conf_t *conf_load(const char *filename);
conf_t *conf_load_blob(const blob_t *buf);

int conf_save(const conf_t *conf, const char *filename);

static inline const conf_section_t *
conf_get_section(const conf_t *conf, int i)
{
    if (i < 0 || i >= conf->sections.len)
        return NULL;
    return conf->sections.tab[i];
}

const conf_section_t *conf_get_section_by_name(const conf_t *conf,
                                               const char *name);

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
int conf_get_verbosity(const conf_t *conf, const char *section,
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
