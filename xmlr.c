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

static __attribute__((format(printf, 2, 3)))
void xmlr_debug_error(void *ctx, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    sb_addvf(ctx, fmt, ap);
    va_end(ap);

#ifndef NDEBUG
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
#endif
}

static void xmlr_initialize(void)
{
    if (unlikely(xmlr_err_g.size == 0))
        sb_init(&xmlr_err_g);
    sb_reset(&xmlr_err_g);
    xmlGenericError = (xmlGenericErrorFunc)xmlr_debug_error;
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

    n[0] = xmlTextReaderCurrentNode(xr);
    for (; n[nlen] && nlen + 1 < countof(n); nlen++) {
        n[nlen + 1] = n[nlen]->parent;
    }
    sb_addf(sb, "%ld", xmlGetLineNo(n[0]));
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
    va_list ap;

    sb_reset(&xmlr_err_g);
    xmlr_fmt_loc(xr, &xmlr_err_g);

    sb_adds(&xmlr_err_g, ": ");
    va_start(ap, fmt);
    sb_addvf(&xmlr_err_g, fmt, ap);
    va_end(ap);
    e_named_trace(1, "xml/reader", "%s", xmlr_err_g.data);

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
          case -1:
            return xmlr_fail(xr, "unable to get node type");
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

int xmlr_node_close(xml_reader_t xr)
{
    if (xmlr_node_is_empty(xr) == 1 || xmlr_node_is_closing(xr) == 1) {
        return xmlr_next_node(xr);
    }
    /* XXX: consider <toto></toto> as autoclosing because libxml doesn't */
    if (!xmlr_on_element(xr, false) || xmlTextReaderRead(xr) < 0) {
        return xmlr_fail(xr, "closing tag expected");
    }
    RETHROW(xmlr_scan_node(xr, true));
    if (xmlr_node_is_empty(xr) == 1 || xmlr_node_is_closing(xr) == 1) {
        return xmlr_next_node(xr);
    }
    return xmlr_fail(xr, "closing tag expected");
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

void xmlr_close(xml_reader_t *xrp)
{
    xml_reader_t xr = *xrp;

    /* when the parsing was interrupted, namespace stack isn't reset properly
     * leading to bugs, so workaround it when we're not sure the parse ended
     * well
     */
    if (xmlTextReaderRead(xr) == 0 &&
        xmlTextReaderReadState(xr) == XML_TEXTREADER_MODE_EOF)
    {
        xmlTextReaderClose(xr);
    } else {
        xmlFreeTextReader(xr);
        *xrp = NULL;
    }
}

int xmlr_node_get_local_name(xml_reader_t xr, lstr_t *out)
{
    const char *s;

    assert (xmlr_on_element(xr, false));
    s = (const char *)xmlTextReaderConstLocalName(xr);
    if (s == NULL) {
        return xmlr_fail(xr, "cannot retrieve char string in %s", __func__);
    }
    *out = LSTR(s);
    return 0;
}

lstr_t xmlr_node_get_xmlns(xml_reader_t xr)
{
    const char *s;

    assert (xmlr_on_element(xr, false));
    s = (const char *)xmlTextReaderConstPrefix(xr);

    return LSTR_OPT(s);
}

lstr_t xmlr_node_get_xmlns_uri(xml_reader_t xr)
{
    const char *s;

    assert (xmlr_on_element(xr, false));
    s = (const char *)xmlTextReaderConstNamespaceUri(xr);

    return LSTR_OPT(s);
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

    if (RETHROW(xmlr_node_is_empty(xr))) {
        xmlr_fail(xr, "node has no children");
        return XMLR_NOCHILD;
    }
    RETHROW(xmlr_next_node(xr));
    if (RETHROW(xmlr_node_is_closing(xr))) {
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

    if (!RETHROW(xmlr_node_is(xr, s, len))) {
        if (flags & XMLR_ENTER_MISSING_OK)
            return 0;
        return xmlr_fail(xr, "expecting tag <%s>", s);
    }

    if (!RETHROW(xmlr_node_is_empty(xr)))
        res = xmlr_next_child(xr);

    if (res == XMLR_NOCHILD) {
        if (flags & XMLR_ENTER_EMPTY_OK)
            return xmlr_next_node(xr);
        return xmlr_fail(xr, "no child in node <%s>", s);
    }
    return res ?: 1;
}

int xmlr_node_skip_until(xml_reader_t xr, const char *s, int len)
{
    while (!RETHROW(xmlr_node_is_closing(xr))) {
        if (RETHROW(xmlr_node_is(xr, s, len)))
            return 0;
        RETHROW(xmlr_next_sibling(xr));
    }
    return xmlr_fail(xr, "missing <%s> tag", s);
}

/* }}} */
/* Reading values {{{ */

__flatten
int xmlr_get_cstr_start(xml_reader_t xr, bool emptyok, lstr_t *out)
{
    const char *s = NULL;

    assert (xmlr_on_element(xr, false));

    if (!RETHROW(xmlr_node_is_empty(xr))) {
        if (RETHROW(xmlTextReaderRead(xr)) != 1)
            return xmlr_fail(xr, "expecting text or closing element");
        RETHROW(xmlr_scan_node(xr, true));
        if (RETHROW(xmlTextReaderNodeType(xr)) == XTYPE(TEXT))
            s = (const char *)xmlTextReaderConstValue(xr);
    }
    if (s) {
        *out = LSTR(s);
    } else {
        if (!emptyok)
            return xmlr_fail(xr, "node value is missing");
        *out = LSTR_EMPTY_V;
    }
    return 0;
}

int xmlr_get_cstr_done(xml_reader_t xr)
{
    if (!RETHROW(xmlr_node_is_empty(xr))) {
        if (RETHROW(xmlTextReaderNodeType(xr)) == XTYPE(TEXT)) {
            RETHROW(xmlr_scan_node(xr, false));
        }
        if (RETHROW(xmlTextReaderNodeType(xr)) != XTYPE(END_ELEMENT))
            return xmlr_fail(xr, "expecting closing tag");
    }
    return xmlr_next_node(xr);
}

static int xmlr_val_strdup(xml_reader_t xr, const char *s,
                           bool emptyok, lstr_t *out)
{
    if (s) {
        *out = lstr_dups(s, strlen(s));
    } else {
        if (!emptyok)
            return xmlr_fail(xr, "node value is missing");
        *out = LSTR_EMPTY_V;
    }
    return 0;
}
#define F(x)    x##_strdup
#define ARGS_P  bool emptyok, lstr_t *out
#define ARGS    emptyok, out
#include "xmlr-get-value.in.c"

static int mp_xmlr_val_strdup(xml_reader_t xr, const char *s,
                              mem_pool_t *mp, bool emptyok, lstr_t *out)
{
    if (s) {
        *out = mp_lstr_dups(mp, s, strlen(s));
    } else {
        if (!emptyok)
            return xmlr_fail(xr, "node value is missing");
        *out = LSTR_EMPTY_V;
    }
    return 0;
}
#define F(x)       mp_##x##_strdup
#define PRE_ARGS_P mem_pool_t *mp,
#define ARGS_P     bool emptyok, lstr_t *out
#define ARGS       mp, emptyok, out
#include "xmlr-get-value.in.c"

static int t_xmlr_val_str(xml_reader_t xr, const char *s,
                          bool emptyok, lstr_t *out)
{
    if (s) {
        *out = t_lstr_dups(s, strlen(s));
    } else {
        if (!emptyok)
            return xmlr_fail(xr, "node value is missing");
        *out = LSTR_EMPTY_V;
    }
    return 0;
}
#define F(x)    t_##x##_str
#define ARGS_P  bool emptyok, lstr_t *out
#define ARGS    emptyok, out
#include "xmlr-get-value.in.c"

static int xmlr_val_int_range_base(xml_reader_t xr, const char *s,
                                   int minv, int maxv, int base, int *ip)
{
    int64_t i64;

    if (!s)
        return xmlr_fail(xr, "node value is missing");
    errno = 0;
    i64 = strtoll(s, &s, base);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "node value is not an integer");
    if (i64 < minv || i64 > maxv)
        return xmlr_fail(xr, "node value isn't in the %d .. %d range", minv, maxv);
    *ip = i64;
    return 0;
}
#define F(x)    x##_int_range_base
#define ARGS_P  int minv, int maxv, int base, int *ip
#define ARGS    minv, maxv, base, ip
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

static int xmlr_val_i64_base(xml_reader_t xr, const char *s, int base,
                             int64_t *i64p)
{
    if (!s)
        return xmlr_fail(xr, "node value is missing");
    errno = 0;
    *i64p = strtoll(s, &s, base);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "node value is not a valid integer");
    return 0;
}
#define F(x)    x##_i64_base
#define ARGS_P  int base, int64_t *i64p
#define ARGS    base, i64p
#include "xmlr-get-value.in.c"

static int xmlr_val_u64_base(xml_reader_t xr, const char *s, int base,
                             uint64_t *u64p)
{
    if (!s)
        return xmlr_fail(xr, "node value is missing");
    errno = 0;
    *u64p = strtoull(s, &s, base);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "node value is not a valid integer");
    return 0;
}
#define F(x)    x##_u64_base
#define ARGS_P  int base, uint64_t *u64p
#define ARGS    base, u64p
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

int xmlr_get_inner_xml(xml_reader_t xr, lstr_t *out)
{
    char *res;

    assert (xmlr_on_element(xr, false));

    res = (char *)xmlTextReaderReadInnerXml(xr);
    if (!res) {
        return xmlr_fail(xr, "cannot retrieve char string in %s", __func__);
    }
    *out = lstr_init_(res, strlen(res), MEM_LIBC);
    return xmlr_next_sibling(xr);
}

int mp_xmlr_get_inner_xml(mem_pool_t *mp, xml_reader_t xr, lstr_t *out)
{
    char *res;

    assert (xmlr_on_element(xr, false));

    res = (char *)xmlTextReaderReadInnerXml(xr);
    if (!res) {
        return xmlr_fail(xr, "cannot retrieve char string in %s", __func__);
    }
    *out = mp_lstr_dups(mp, res, strlen(res));
    free(res);
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
                           bool emptyok, lstr_t *out)
{
    if (s) {
        *out = t_lstr_dups(s, strlen(s));
    } else {
        if (!emptyok)
            return xmlr_fail(xr, "[%s] value is missing", name);
        *out = LSTR_EMPTY_V;
    }
    return 0;
}
#define F(x)    t_##x##_str
#define ARGS_P  bool emptyok, lstr_t *out
#define ARGS    emptyok, out
#include "xmlr-get-attr.in.c"

static int
xmlr_attr_int_range_base(xml_reader_t xr, const char *name, const char *s,
                         int minv, int maxv, int base, int *ip)
{
    int64_t i64;

    if (!s)
        return xmlr_fail(xr, "[%s] value is missing", name);
    errno = 0;
    i64 = strtoll(s, &s, base);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "[%s] value is not an integer", name);
    if (i64 < minv || i64 > maxv)
        return xmlr_fail(xr, "[%s] value isn't in the %d .. %d range", name, minv, maxv);
    *ip = i64;
    return 0;
}
#define F(x)    x##_int_range_base
#define ARGS_P  int minv, int maxv, int base, int *ip
#define ARGS    minv, maxv, base, ip
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

static int xmlr_attr_i64_base(xml_reader_t xr, const char *name,
                              const char *s, int base, int64_t *i64p)
{
    if (!s)
        return xmlr_fail(xr, "[%s] value is missing", name);
    errno = 0;
    *i64p = strtoll(s, &s, base);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "[%s] value is not a valid integer", name);
    return 0;
}
#define F(x)    x##_i64_base
#define ARGS_P  int base, int64_t *i64p
#define ARGS    base, i64p
#include "xmlr-get-attr.in.c"

static int xmlr_attr_u64_base(xml_reader_t xr, const char *name,
                              const char *s, int base, uint64_t *u64p)
{
    if (!s)
        return xmlr_fail(xr, "[%s] value is missing", name);
    errno = 0;
    *u64p = strtoull(s, &s, base);
    if (skipspaces(s)[0] || errno)
        return xmlr_fail(xr, "[%s] value is not a valid integer", name);
    return 0;
}
#define F(x)    x##_u64_base
#define ARGS_P  int base, uint64_t *u64p
#define ARGS    base, u64p
#include "xmlr-get-attr.in.c"

static int xmlr_attr_dbl(xml_reader_t xr, const char *name, const char *s,
                         double *dblp)
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
/* {{{ Tests */

#include "z.h"

Z_GROUP_EXPORT(xmlr)
{
    Z_TEST(xmlr_node_get_xmlns, "xmlr_node_get_xmlns") {
        lstr_t body = LSTR("<ns:elt xmlns:ns=\"ns_uri\" />");
        lstr_t name;
        lstr_t ns;

        Z_ASSERT(xmlr_setup(&xmlr_g, body.s, body.len) >= 0);

        Z_ASSERT(xmlr_node_get_local_name(xmlr_g, &name) >= 0);
        Z_ASSERT_STREQUAL(name.s, "elt");

        ns = xmlr_node_get_xmlns(xmlr_g);
        Z_ASSERT_STREQUAL(ns.s, "ns");

        xmlr_close(&xmlr_g);
    } Z_TEST_END;

    Z_TEST(xmlr_node_get_xmlns_uri, "xmlr_node_get_xmlns_uri") {
        lstr_t body = LSTR("<ns:elt xmlns:ns=\"ns_uri\" />");
        lstr_t name;
        lstr_t ns_uri;

        Z_ASSERT(xmlr_setup(&xmlr_g, body.s, body.len) >= 0);

        Z_ASSERT(xmlr_node_get_local_name(xmlr_g, &name) >= 0);
        Z_ASSERT_STREQUAL(name.s, "elt");

        ns_uri = xmlr_node_get_xmlns_uri(xmlr_g);
        Z_ASSERT_STREQUAL(ns_uri.s, "ns_uri");

        xmlr_close(&xmlr_g);
    } Z_TEST_END;

    Z_TEST(xmlr_node_get_xmlns_no_uri, "xmlr_node_get_xmlns_no_uri") {
        lstr_t body = LSTR("<elt xmlns:ns=\"ns_uri\" />");
        lstr_t name;
        lstr_t ns_uri;

        Z_ASSERT(xmlr_setup(&xmlr_g, body.s, body.len) >= 0);

        Z_ASSERT(xmlr_node_get_local_name(xmlr_g, &name) >= 0);
        Z_ASSERT_STREQUAL(name.s, "elt");

        ns_uri = xmlr_node_get_xmlns_uri(xmlr_g);
        Z_ASSERT(ns_uri.len == 0);

        xmlr_close(&xmlr_g);
    } Z_TEST_END;

    Z_TEST(node_should_close, "") {
        t_scope;
        lstr_t xml = LSTR("<root>                                       "
                          "    <child1>                                 "
                          "       <granchild>value_granchild</granchild>"
                          "    </child1>                                "
                          "    <child2>value_child2</child2>            "
                          "    <child3 attr=\"autoclosing\" />          "
                          "    <child4><!--empty-->  </child4>          "
                          " </root>");
        lstr_t val = LSTR_NULL;

        xmlr_setup(&xmlr_g, xml.s, xml.len);
        Z_ASSERT_EQ(xmlr_node_open_s(xmlr_g, "root"), 1);
        Z_ASSERT_EQ(xmlr_node_open_s(xmlr_g, "child1"), 1);
        Z_ASSERT_ZERO(t_xmlr_get_str(xmlr_g, false, &val));
        Z_ASSERT_LSTREQUAL(val, LSTR("value_granchild"));
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g)); /* </child1> */
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child2"), 1);
        Z_ASSERT_ZERO(xmlr_node_is_closing(xmlr_g)); /* not empty */
        Z_ASSERT_ZERO(t_xmlr_get_str(xmlr_g, false, &val));
        Z_ASSERT_LSTREQUAL(val, LSTR("value_child2"));
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child3"), 1);
        /* </child3> autoclosing, should close */
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g));
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child4"), 1);
        /* </child4> empty, should close */
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g));
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g)); /* </root> should close */
    } Z_TEST_END;

    Z_TEST(node_should_not_close, "") {
        lstr_t xml = LSTR("<root>"
                          "    <child>value_child</child>"
                          "</root>");

        xmlr_setup(&xmlr_g, xml.s, xml.len);
        Z_ASSERT_EQ(xmlr_node_open_s(xmlr_g, "root"), 1);
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child"), 1);
        Z_ASSERT_NEG(xmlr_node_close(xmlr_g));
    } Z_TEST_END;

    Z_TEST(node_should_close_2, "") {
        lstr_t xml = LSTR("<root>"
                          "    <child>value_child</child>"
                          "</root>");

        xmlr_setup(&xmlr_g, xml.s, xml.len);
        Z_ASSERT_EQ(xmlr_node_open_s(xmlr_g, "root"), 1);
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child"), 1);
        Z_ASSERT_ZERO(xmlr_node_skip_s(xmlr_g, "child", XMLR_ENTER_EMPTY_OK));
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g));
    } Z_TEST_END;
} Z_GROUP_END;

/* }}} */
