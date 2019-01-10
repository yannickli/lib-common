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

#ifndef IS_LIB_INET_XMLR_H
#define IS_LIB_INET_XMLR_H

#include <libxml/xmlreader.h>
#include <lib-common/core.h>

typedef xmlTextReaderPtr xml_reader_t;

extern __thread xml_reader_t xmlr_g;

enum xmlr_error {
    XMLR_OK      = 0,
    XMLR_ERROR   = -1,
    XMLR_NOCHILD = -2,
};

#define XMLR_CHECK(expr, on_err) \
    ({ int __xres = (expr); if (unlikely(__xres < 0)) on_err; __xres; })

/* \brief Initiates the parser with the content in the buffer.
 *
 * This function wants to position itself (pre-load) the root node of the
 * document.
 */
int xmlr_setup(xml_reader_t *xrp, const void *buf, int len);

static inline void xmlr_close(xml_reader_t xr)
{
    xmlTextReaderClose(xr);
}

static inline void xmlr_delete(xml_reader_t *xrp)
{
    if (*xrp) {
        xmlFreeTextReader(*xrp);
        *xrp = NULL;
    }
}

__cold
int  xmlr_fail(xml_reader_t xr, const char *fmt, ...) __attr_printf__(2, 3);
void xmlr_clear_err(void);
__cold
const char *xmlr_get_err(void);

/* \brief Goes to the next node element (closing or opening)
 */
int xmlr_next_node(xml_reader_t xr);

/* \brief Goes to the first child of the current node
 */
int xmlr_next_child(xml_reader_t xr);

/* \brief Skip the current node fully, and goes to the next one.
 */
int xmlr_next_sibling(xml_reader_t xr);

/* \brief Skip to the next node after the end of the one at the current level.
 */
int xmlr_next_uncle(xml_reader_t xr);

static inline int xmlr_node_is_empty(xml_reader_t xr)
{
    return RETHROW(xmlTextReaderIsEmptyElement(xr));
}

static inline int xmlr_node_is_closing(xml_reader_t xr)
{
    return RETHROW(xmlTextReaderNodeType(xr)) == XML_READER_TYPE_END_ELEMENT;
}

static inline int xmlr_node_is(xml_reader_t xr, const char *s, size_t len)
{
    const char *s2;

    if (RETHROW(xmlr_node_is_closing(xr)))
        return false;
    s2 = (const char *)xmlTextReaderConstLocalName(xr);
    if (s2 == NULL)
        return XMLR_ERROR;
    if (len != strlen(s2))
        return false;
    return memcmp(s, s2, len) == 0;
}
static inline int xmlr_node_is_s(xml_reader_t xr, const char *s)
{
    return xmlr_node_is(xr, s, strlen(s));
}

static inline int xmlr_node_want(xml_reader_t xr, const char *s, size_t len)
{
    if (!xmlr_node_is(xr, s, len))
        return xmlr_fail(xr, "missing <%s> tag", s);
    return 0;
}
static inline int xmlr_node_want_s(xml_reader_t xr, const char *s)
{
    return xmlr_node_want(xr, s, strlen(s));
}

#define XMLR_ENTER_MISSING_OK    (1U << 0)
#define XMLR_ENTER_EMPTY_OK      (2U << 0)
#define XMLR_ENTER_ALL_OK        (0xffffffff)
int xmlr_node_enter(xml_reader_t xr, const char *s, size_t len, int flags);
static inline int xmlr_node_enter_s(xml_reader_t xr, const char *s, int flags)
{
    return xmlr_node_enter(xr, s, strlen(s), flags);
}

static inline int xmlr_node_try_open_s(xml_reader_t xr, const char *s)
{
    return xmlr_node_enter_s(xr, s, XMLR_ENTER_ALL_OK);
}

static inline int xmlr_node_open_s(xml_reader_t xr, const char *s)
{
    return xmlr_node_enter_s(xr, s, 0);
}

static inline int xmlr_node_close(xml_reader_t xr)
{
    if (xmlr_node_is_empty(xr) == 1 || xmlr_node_is_closing(xr) == 1)
        return xmlr_next_node(xr);
    /* check if next tag is the closing tag */
    RETHROW(xmlr_next_node(xr));
    if (xmlr_node_is_closing(xr) == 1)
        return xmlr_next_node(xr);
    return xmlr_fail(xr, "closing tag expected");
}

static inline int xmlr_node_close_n(xml_reader_t xr, size_t n)
{
    while (n-- > 0)
        RETHROW(xmlr_node_close(xr));
    return 0;
}

static inline int xmlr_node_skip_s(xml_reader_t xr, const char *s, int flags)
{
    if (RETHROW(xmlr_node_enter_s(xr, s, flags)))
        return xmlr_next_uncle(xr);
    return 0;
}


/* \brief Get the current node value, and go to the next node.
 *
 * This function fails if the node has childen.
 */
int xmlr_get_cstr_start(xml_reader_t xr, bool nullok, const char **out, int *lenp);
int xmlr_get_cstr_done(xml_reader_t xr);

int xmlr_get_strdup(xml_reader_t xr, bool nullok, char **out, int *lenp);
int t_xmlr_get_str(xml_reader_t xr, bool nullok, char **out, int *lenp);
static ALWAYS_INLINE
int t_xmlr_get_cstr(xml_reader_t xr, bool nullok, const char **out, int *lenp)
{
    return t_xmlr_get_str(xr, nullok, (char **)out, lenp);
}

int xmlr_get_int_range(xml_reader_t xr, int minv, int maxv, int *ip);
int xmlr_get_bool(xml_reader_t xr, bool *bp);
int xmlr_get_i64(xml_reader_t xr, int64_t *i64p);
int xmlr_get_u64(xml_reader_t xr, uint64_t *u64p);
int xmlr_get_dbl(xml_reader_t xr, double *dp);

int xmlr_get_inner_xml(xml_reader_t xr, char **out, int *lenp);

/* attributes stuff */

#define xmlr_for_each_attr(attr, xr) \
    for (xmlAttrPtr attr = xmlTextReaderCurrentNode(xr)->properties; \
         attr; attr = attr->next)

xmlAttrPtr
xmlr_find_attr(xml_reader_t xr, const char *name, size_t len, bool needed);
static ALWAYS_INLINE xmlAttrPtr
xmlr_find_attr_s(xml_reader_t xr, const char *name, bool needed)
{
    return xmlr_find_attr(xr, name, strlen(name), needed);
}

/* \brief Get the current node attribute value.
 */
int t_xmlr_getattr_str(xml_reader_t xr, xmlAttrPtr attr,
                       bool nullok, char **out, int *lenp);
static ALWAYS_INLINE int
t_xmlr_getattr_cstr(xml_reader_t xr, xmlAttrPtr attr,
                    bool nullok, const char **out, int *lenp)
{
    return t_xmlr_getattr_str(xr, attr, nullok, (char **)out, lenp);
}

int xmlr_getattr_int_range(xml_reader_t xr, xmlAttrPtr attr,
                           int minv, int maxv, int *ip);
int xmlr_getattr_bool(xml_reader_t xr, xmlAttrPtr attr, bool *bp);
int xmlr_getattr_i64(xml_reader_t xr, xmlAttrPtr attr, int64_t *i64p);
int xmlr_getattr_u64(xml_reader_t xr, xmlAttrPtr attr, uint64_t *u64p);
int xmlr_getattr_dbl(xml_reader_t xr, xmlAttrPtr attr, double *dp);

#endif
