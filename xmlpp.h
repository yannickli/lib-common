/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_XMLPP_H
#define IS_LIB_COMMON_XMLPP_H

#include "container-qvector.h"

typedef struct xmlpp_t {
    bool can_do_attr : 1;
    bool was_a_tag   : 1;
    bool nospace     : 1;
    sb_t  *buf;
    qv_t(lstr) stack;
} xmlpp_t;

void xmlpp_open_banner(xmlpp_t *, sb_t *buf);
void xmlpp_open(xmlpp_t *, sb_t *buf);
void xmlpp_close(xmlpp_t *);

void xmlpp_opentag(xmlpp_t *, const char *tag);
void xmlpp_closetag(xmlpp_t *);

static inline xmlpp_t *
__xmlpp_open_tag_and_return(xmlpp_t *xmlpp, const char *tag)
{
    xmlpp_opentag(xmlpp, tag);

    return xmlpp;
}

#define ____xmlpp_tag_scope(_tmpname, _xmlpp, _tag)                          \
    for (xmlpp_t *_tmpname = __xmlpp_open_tag_and_return((_xmlpp), (_tag));  \
         _tmpname; xmlpp_closetag(_tmpname), _tmpname = NULL)

#define ___xmlpp_tag_scope(n, _xmlpp, _tag)                                  \
    ____xmlpp_tag_scope(__xmlpp_scope_##n, (_xmlpp), (_tag))

/* XXX This level of macro is just here to force the preprocessing of
 * predefined macro "__LINE__". */
#define __xmlpp_tag_scope(n, _xmlpp, _tag)                                   \
    ___xmlpp_tag_scope(n, (_xmlpp), (_tag))

#define xmlpp_tag_scope(_xmlpp, _tag)                                        \
    __xmlpp_tag_scope(__LINE__, (_xmlpp), (_tag))

void xmlpp_nl(xmlpp_t *);

void xmlpp_putattr(xmlpp_t *, const char *key, const char *val);
void xmlpp_putattrfmt(xmlpp_t *, const char *key,
                      const char *fmt, ...) __attr_printf__(3, 4);

void xmlpp_put_cdata(xmlpp_t *, const char *s, size_t len);
void xmlpp_put(xmlpp_t *, const void *data, int len);
static inline void xmlpp_puts(xmlpp_t *pp, const char *s) {
    xmlpp_put(pp, s, strlen(s));
}
static inline void xmlpp_put_lstr(xmlpp_t *pp, lstr_t s)
{
    xmlpp_put(pp, s.s, s.len);
}
void xmlpp_putf(xmlpp_t *, const char *fmt, ...) __attr_printf__(2, 3);


static inline void xmlpp_closentag(xmlpp_t *pp, int n) {
    while (n-- > 0)
        xmlpp_closetag(pp);
}

static inline void xmlpp_opensib(xmlpp_t *pp, const char *tag) {
    xmlpp_closetag(pp);
    xmlpp_opentag(pp, tag);
}

#endif /* IS_LIB_COMMON_XMLPP_H */
