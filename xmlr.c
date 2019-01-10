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

#include "xmlr.h"
#include "thr.h"

/* work around a bug in early 4.6 gcc releases */
#if __GNUC__ == 4 && __GNUC_MINOR__ == 6 && __GNUC_PATCHLEVEL__ <= 1
# undef __flatten
# define __flatten
#endif

/*
 * This libxml wrapper supposes the following:
 *
 * - all is utf8
 * - there is no meaningful interleaved text and nodes like in html:
 *   <p>text 1<b>some b node</b>text2</p>
 *
 *
 * The following invariant is maintained: at any given time, the xml_reader_t
 * points onto an opening or closing element, except when the parsing failed
 * for some reason.
 *
 */

__thread xml_reader_t xmlr_g;

#define XMLR_OPTS \
    (XML_PARSE_NOERROR | XML_PARSE_NOWARNING | \
     XML_PARSE_NONET | XML_PARSE_NOCDATA | XML_PARSE_NOBLANKS)

#define XTYPE(type)  XML_READER_TYPE_##type

/* Error formatting {{{ */

static __thread sb_t xmlr_err_g;

#ifndef NDEBUG
static __attribute__((format(printf, 2, 3)))
void xmlr_debug_error(void *ctx, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    sb_addvf(ctx, fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
#endif

static void xmlr_initialize(void)
{
    if (unlikely(xmlr_err_g.size == 0))
        sb_init(&xmlr_err_g);
    sb_reset(&xmlr_err_g);
#ifndef NDEBUG
    xmlGenericError = (xmlGenericErrorFunc)xmlr_debug_error;
#else
    xmlGenericError = (xmlGenericErrorFunc)sb_addf;
#endif
    xmlGenericErrorContext = &xmlr_err_g;
}

static void xmlr_shutdown(void)
{
    xmlr_clear_err();
    xmlr_delete(&xmlr_g);
}
thr_hooks(NULL, xmlr_shutdown);

void xmlr_clear_err(void)
{
    if (xmlr_err_g.size)
        sb_wipe(&xmlr_err_g);
}

const char *xmlr_get_err(void)
{
    if (xmlr_err_g.len)
        return xmlr_err_g.data;
    return NULL;
}


static void xmlr_fmt_loc(xml_reader_t xr, sb_t *sb)
{
    xmlNodePtr n[32];
    int nlen = 0;
    int col, lno;

    n[0] = xmlTextReaderCurrentNode(xr);
    for (; n[nlen] && nlen < countof(n); nlen++) {
        n[nlen + 1] = n[nlen]->parent;
    }
    lno = xmlTextReaderGetParserLineNumber(xr);
    col = xmlTextReaderGetParserColumnNumber(xr);
    if (lno && col) {
        sb_addf(sb, "%d:%d", lno, col);
    } else {
        sb_addf(sb, "pos %ld", xmlTextReaderByteConsumed(xr));
    }
    if (nlen) {
        if (nlen == countof(n) && n[nlen - 1]->parent) {
            sb_adds(sb, ": near ...");
        } else {
            sb_adds(sb, ": near ");
        }
        for (int i = nlen; i-- > 0; ) {
            if (n[i]->type == XML_ELEMENT_NODE) {
                sb_addf(sb, "/%s", (const char *)n[i]->name);
            } else
            if (n[i]->type == XML_TEXT_NODE) {
                assert (i == 0);
                sb_adds(sb, "/text()");
            } else
            if (n[i]->type == XML_DOCUMENT_NODE) {
                assert (i == nlen - 1);
            } else {
                assert (false);
            }
        }
    }
}

int xmlr_fail(xml_reader_t xr, const char *fmt, ...)
{
    sb_reset(&xmlr_err_g);
    xmlr_fmt_loc(xr, &xmlr_err_g);
    if (fmt) {
        va_list ap;

        sb_adds(&xmlr_err_g, ": ");
        va_start(ap, fmt);
        sb_addvf(&xmlr_err_g, fmt, ap);
        va_end(ap);
#ifndef NDEBUG
        e_trace(0, "%s", xmlr_err_g.data);
#endif
    }
    return XMLR_ERROR;
}

static void
xmlr_err_handler(void *arg, const char *msg, xmlParserSeverities lvl,
                 xmlTextReaderLocatorPtr loc)
{
    sb_reset(&xmlr_err_g);
    xmlParserError(loc, "%s", msg);
}

/* }}} */
/* Crawling {{{ */

static ALWAYS_INLINE bool xmlr_on_element(xml_reader_t xr, bool allow_closing)
{
    switch (xmlTextReaderNodeType(xr)) {
      case XTYPE(ELEMENT):
        return true;
      case XTYPE(END_ELEMENT):
        return allow_closing;
      default:
        return false;
    }
}

static ALWAYS_INLINE int xmlr_scan_node(xml_reader_t xr, bool stop_on_text)
{
    for (;;) {
        switch (xmlTextReaderNodeType(xr)) {
          case XTYPE(ELEMENT):
          case XTYPE(END_ELEMENT):
          case XTYPE(NONE):
            return 0;
          case XTYPE(TEXT):
            if (stop_on_text)
                return 0;
            break;
          default:
            break;
        }
        RETHROW(xmlTextReaderRead(xr));
    }
}

int xmlr_setup(xml_reader_t *xrp, const void *buf, int len)
{
    xml_reader_t xr = *xrp;

    xmlr_initialize();

    if (xr) {
        if (xmlReaderNewMemory(xr, buf, len, NULL, NULL, XMLR_OPTS) < 0) {
            xmlFreeTextReader(xr);
            *xrp = xr = NULL;
            return xmlr_fail(xr, "unable to allocate parser");
        }
    } else {
        *xrp = xr = xmlReaderForMemory(buf, len, NULL, NULL, XMLR_OPTS);
        if (!xr)
            return xmlr_fail(xr, "unable to allocate parser");
    }

    xmlTextReaderSetErrorHandler(xr, xmlr_err_handler, xr);
    if (RETHROW(xmlTextReaderRead(xr)) != 1)
        return xmlr_fail(xr, "unable to load root node");
    return xmlr_scan_node(xr, false);
}

int xmlr_next_node(xml_reader_t xr)
{
    assert (xmlr_on_element(xr, true));

    RETHROW(xmlTextReaderRead(xr));
    return xmlr_scan_node(xr, false);
}

int xmlr_next_child(xml_reader_t xr)
{
    assert (xmlr_on_element(xr, false));

    if (xmlr_node_is_empty(xr)) {
        xmlr_fail(xr, "node has no children");
        return XMLR_NOCHILD;
    }
    RETHROW(xmlr_next_node(xr));
    if (xmlr_node_is_closing(xr)) {
        xmlr_fail(xr, "node has no children");
        return XMLR_NOCHILD;
    }
    return 0;
}

int xmlr_next_sibling(xml_reader_t xr)
{
    assert (xmlr_on_element(xr, true));

    if (RETHROW(xmlTextReaderNext(xr)) == 1)
        return xmlr_scan_node(xr, false);
    return xmlr_fail(xr, "node has no sibling");
}

int xmlr_next_uncle(xml_reader_t xr)
{
    while (!RETHROW(xmlr_node_is_closing(xr))) {
        RETHROW(xmlr_next_sibling(xr));
    }
    return xmlr_next_node(xr);
}

int xmlr_node_enter(xml_reader_t xr, const char *s, size_t len, int flags)
{
    int res = XMLR_NOCHILD;

    if (!xmlr_node_is(xr, s, len)) {
        if (flags & XMLR_ENTER_MISSING_OK)
            return 0;
        return xmlr_fail(xr, "expecting tag <%s>", s);
    }

    if (!xmlr_node_is_empty(xr))
        res = xmlr_next_child(xr);

    if (res == XMLR_NOCHILD) {
        if (flags & XMLR_ENTER_EMPTY_OK)
            return xmlr_next_node(xr);
        return XMLR_ERROR;
    }
    return res ?: 1;
}

/* }}} */
/* Reading values {{{ */

#ifndef __clang__
__attribute__((flatten))
#endif
int xmlr_get_cstr_start(xml_reader_t xr,
                        bool nullok, const char **out, int *lenp)
{
    const char *s = NULL;

    assert (xmlr_on_element(xr, false));

    if (!xmlTextReaderIsEmptyElement(xr)) {
        if (RETHROW(xmlTextReaderRead(xr)) != 1)
            return xmlr_fail(xr, "expecting text or closing element");
        RETHROW(xmlr_scan_node(xr, true));
        if (xmlTextReaderNodeType(xr) == XTYPE(TEXT))
            s = (const char *)xmlTextReaderConstValue(xr);
    }
    if (s) {
        if (lenp)
            *lenp = strlen(s);
    } else {
        if (!nullok)
            return xmlr_fail(xr, "node value is missing");
        if (lenp)
            *lenp = 0;
    }
    *out = s;
    return 0;
}

int xmlr_get_cstr_done(xml_reader_t xr)
{
    if (!xmlTextReaderIsEmptyElement(xr)) {
        if (xmlTextReaderNodeType(xr) == XTYPE(TEXT)) {
            RETHROW(xmlr_scan_node(xr, false));
        }
        if (xmlTextReaderNodeType(xr) != XTYPE(END_ELEMENT))
            return xmlr_fail(xr, "expecting closing tag");
    }
    return xmlr_next_node(xr);
}

static int xmlr_val_strdup(xml_reader_t xr, const char *s,
                           bool nullok, char **out, int *lenp)
{
    int len;

    if (s) {
        len  = strlen(s);
        *out = p_dupz(s, len);
    } else {
        if (!nullok)
            return xmlr_fail(xr, "node value is missing");
        len  = 0;
        *out = NULL;
    }
    if (lenp)
        *lenp = len;
    return 0;
}
#define F(x)    x##_strdup
#define ARGS_P  bool nullok, char **out, int *lenp
#define ARGS    nullok, out, lenp
#include "xmlr-get-value.in.c"

static int t_xmlr_val_str(xml_reader_t xr, const char *s,
                          bool nullok, char **out, int *lenp)
{
    int len;

    if (s) {
        len  = strlen(s);
        *out = t_dupz(s, len);
    } else {
        if (!nullok)
            return xmlr_fail(xr, "node value is missing");
        len  = 0;
        *out = NULL;
    }
    if (lenp)
        *lenp = len;
    return 0;
}
#define F(x)    t_##x##_str
#define ARGS_P  bool nullok, char **out, int *lenp
#define ARGS    nullok, out, lenp
#include "xmlr-get-value.in.c"

static int xmlr_val_int_range(xml_reader_t xr, const char *s,
                              int minv, int maxv, int *ip)
{
    int64_t i64;

    if (!s)
        return xmlr_fail(xr, "node value is missing");
    errno = 0;
    i64 = strtoll(s, &s, 10);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "node value is not an integer");
    if (i64 < minv || i64 > maxv)
        return xmlr_fail(xr, "node value isn't in the %d .. %d range", minv, maxv);
    *ip = i64;
    return 0;
}
#define F(x)    x##_int_range
#define ARGS_P  int minv, int maxv, int *ip
#define ARGS    minv, maxv, ip
#include "xmlr-get-value.in.c"

static int xmlr_val_bool(xml_reader_t xr, const char *s, bool *bp)
{
    if (!s)
        return xmlr_fail(xr, "node value is missing");
    s = skipspaces(s);
    if (*s == '0' || *s == '1') {
        *bp = *s++ - '0';
    } else
    if (stristart(s, "true", &s)) {
        *bp = true;
    } else
    if (stristart(s, "false", &s)) {
        *bp = false;
    } else {
        return xmlr_fail(xr, "node value is not a valid boolean");
    }
    if (skipspaces(s)[0])
        return xmlr_fail(xr, "node value is not a valid boolean");
    return 0;
}
#define F(x)    x##_bool
#define ARGS_P  bool *bp
#define ARGS    bp
#include "xmlr-get-value.in.c"

static int xmlr_val_i64(xml_reader_t xr, const char *s, int64_t *i64p)
{
    if (!s)
        return xmlr_fail(xr, "node value is missing");
    errno = 0;
    *i64p = strtoll(s, &s, 10);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "node value is not a valid integer");
    return 0;
}
#define F(x)    x##_i64
#define ARGS_P  int64_t *i64p
#define ARGS    i64p
#include "xmlr-get-value.in.c"

static int xmlr_val_u64(xml_reader_t xr, const char *s, uint64_t *u64p)
{
    if (!s)
        return xmlr_fail(xr, "node value is missing");
    errno = 0;
    *u64p = strtoull(s, &s, 10);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "node value is not a valid integer");
    return 0;
}
#define F(x)    x##_u64
#define ARGS_P  uint64_t *u64p
#define ARGS    u64p
#include "xmlr-get-value.in.c"

static int xmlr_val_dbl(xml_reader_t xr, const char *s, double *dblp)
{
    if (!s)
        return xmlr_fail(xr, "node value is missing");
    errno = 0;
    *dblp = strtod(s, (char **)&s);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "node value is not a valid number");
    return 0;
}
#define F(x)    x##_dbl
#define ARGS_P  double *dblp
#define ARGS    dblp
#include "xmlr-get-value.in.c"

int xmlr_get_inner_xml(xml_reader_t xr, char **out, int *lenp)
{
    char *res;

    assert (xmlr_on_element(xr, false));

    res = (char *)xmlTextReaderReadInnerXml(xr);
    if (!res)
        return XMLR_ERROR;
    *out = res;
    if (lenp)
        *lenp = strlen(res);
    return xmlr_next_sibling(xr);
}

/* }}} */
/* Reading attributes {{{ */

xmlAttrPtr
xmlr_find_attr(xml_reader_t xr, const char *name, size_t len, bool needed)
{
    xmlr_for_each_attr(attr, xr) {
        if (strlen((char *)attr->name) != len)
            continue;
        if (memcmp(attr->name, name, len))
            continue;
        return attr;
    }
    if (needed)
        xmlr_fail(xr, "missing [%s] attribute", name);
    return NULL;
}

static int t_xmlr_attr_str(xml_reader_t xr, const char *name, const char *s,
                           bool nullok, char **out, int *lenp)
{
    int len;

    if (s) {
        len  = strlen(s);
        *out = t_dupz(s, len);
    } else {
        if (!nullok)
            return xmlr_fail(xr, "[%s] value is missing", name);
        len  = 0;
        *out = NULL;
    }
    if (lenp)
        *lenp = len;
    return 0;
}
#define F(x)    t_##x##_str
#define ARGS_P  bool nullok, char **out, int *lenp
#define ARGS    nullok, out, lenp
#include "xmlr-get-attr.in.c"

static int xmlr_attr_int_range(xml_reader_t xr, const char *name, const char *s,
                              int minv, int maxv, int *ip)
{
    int64_t i64;

    if (!s)
        return xmlr_fail(xr, "[%s] value is missing", name);
    errno = 0;
    i64 = strtoll(s, &s, 10);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "[%s] value is not an integer", name);
    if (i64 < minv || i64 > maxv)
        return xmlr_fail(xr, "[%s] value isn't in the %d .. %d range", name, minv, maxv);
    *ip = i64;
    return 0;
}
#define F(x)    x##_int_range
#define ARGS_P  int minv, int maxv, int *ip
#define ARGS    minv, maxv, ip
#include "xmlr-get-attr.in.c"

static int xmlr_attr_bool(xml_reader_t xr, const char *name, const char *s, bool *bp)
{
    if (!s)
        return xmlr_fail(xr, "[%s] value is missing", name);
    s = skipspaces(s);
    if (*s == '0' || *s == '1') {
        *bp = *s++ - '0';
    } else
    if (stristart(s, "true", &s)) {
        *bp = true;
    } else
    if (stristart(s, "false", &s)) {
        *bp = false;
    } else {
        return xmlr_fail(xr, "[%s] value is not a valid boolean", name);
    }
    if (skipspaces(s)[0])
        return xmlr_fail(xr, "[%s] value is not a valid boolean", name);
    return 0;
}
#define F(x)    x##_bool
#define ARGS_P  bool *bp
#define ARGS    bp
#include "xmlr-get-attr.in.c"

static int xmlr_attr_i64(xml_reader_t xr, const char *name, const char *s, int64_t *i64p)
{
    if (!s)
        return xmlr_fail(xr, "[%s] value is missing", name);
    errno = 0;
    *i64p = strtoll(s, &s, 10);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "[%s] value is not a valid integer", name);
    return 0;
}
#define F(x)    x##_i64
#define ARGS_P  int64_t *i64p
#define ARGS    i64p
#include "xmlr-get-attr.in.c"

static int xmlr_attr_u64(xml_reader_t xr, const char *name, const char *s, uint64_t *u64p)
{
    if (!s)
        return xmlr_fail(xr, "[%s] value is missing", name);
    errno = 0;
    *u64p = strtoull(s, &s, 10);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "[%s] value is not a valid integer", name);
    return 0;
}
#define F(x)    x##_u64
#define ARGS_P  uint64_t *u64p
#define ARGS    u64p
#include "xmlr-get-attr.in.c"

static int xmlr_attr_dbl(xml_reader_t xr, const char *name, const char *s, double *dblp)
{
    if (!s)
        return xmlr_fail(xr, "[%s] value is missing", name);
    errno = 0;
    *dblp = strtod(s, (char **)&s);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "[%s] value is not a valid number", name);
    return 0;
}
#define F(x)    x##_dbl
#define ARGS_P  double *dblp
#define ARGS    dblp
#include "xmlr-get-attr.in.c"

/* }}} */
