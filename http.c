/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "unix.h"
#include "time.h"

#include "http.h"
#include "httptokens.h"

/*
 * rfc 2616 TODO list:
 *
 * ETags
 * Range requests
 *
 * Support Connection: close + no Content-Length as per RFC
 *
 * Automatically transform chunked-encoding to C-L for HTTP/1.0
 *
 */

struct http_params http_params_g = {
#define _G  http_params_g
    .header_line_max    = 1024,
    .header_size_max    = 64 << 10,
    .outbuf_max_size    = 32 << 20,
    .on_data_threshold  = BUFSIZ,
    .pipeline_depth_in  = 32,
    .pipeline_depth_out = 32,
    .noact_delay        = 30 * 1000,
    .max_queries_in     = 1024,
    .max_queries_out    = 1024,
    .max_conns_in       = 1000,
};

enum http_parse_code {
    PARSE_MISSING_DATA =  1,
    PARSE_OK           =  0,
    PARSE_ERROR        = -1,
};

struct http_date {
    time_t date;
    char   buf[sizeof("Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n")];
};
static __thread struct http_date date_cache_g;

/* "()<>@,;:\<>/[]?={} \t" + 1..31 + DEL  */
static ctype_desc_t const http_non_token = {
    {
        0xffffffff, 0xfc009301, 0x38000001, 0xa8000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
    }
};

static void httpd_mark_query_answered(httpd_query_t *q);

static void httpd_trigger_destroy(httpd_trigger_t *cb, unsigned delta)
{
    assert (cb->refcnt >= delta);

    cb->refcnt -= delta;
    if (cb->refcnt == 0) {
        lstr_wipe(&cb->auth_realm);
        if (cb->destroy) {
            cb->destroy(cb);
        } else {
            p_delete(&cb);
        }
    }
}

static httpd_trigger_t *httpd_trigger_dup(httpd_trigger_t *cb)
{
    cb->refcnt += 2;
    return cb;
}

static void httpd_trigger_delete(httpd_trigger_t **cbp)
{
    if (*cbp) {
        httpd_trigger_destroy(*cbp, 2);
        *cbp = NULL;
    }
}

void httpd_trigger_persist(httpd_trigger_t *cb)
{
    cb->refcnt |= 1;
}

void httpd_trigger_loose(httpd_trigger_t *cb)
{
    httpd_trigger_destroy(cb, cb->refcnt & 1);
}

/* RFC 2616 helpers {{{ */

#define PARSE_RETHROW(e)  ({ int __e = (e); if (unlikely(__e)) return __e; })

static inline void http_skipspaces(pstream_t *ps)
{
    while (ps->s < ps->s_end && (ps->s[0] == ' ' || ps->s[0] == '\t'))
        ps->s++;
}

/* rfc 2616, §2.2: Basic rules */
static inline int http_getline(pstream_t *ps, pstream_t *out)
{
    const char *p = memmem(ps->s, ps_len(ps), "\r\n", 2);

    if (unlikely(!p)) {
        *out = ps_initptr(NULL, NULL);
        if (ps_len(ps) > _G.header_line_max)
            return PARSE_ERROR;
        return PARSE_MISSING_DATA;
    }
    *out = ps_initptr(ps->s, p);
    return __ps_skip_upto(ps, p + 2);
}

/* rfc 2616, §3.3.1: Full Date */
static char const * const days[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};
static char const * const months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static inline void http_update_date_cache(struct http_date *out, time_t now)
{
    if (out->date != now) {
        struct tm tm;

        gmtime_r(&now, &tm);
        sprintf(out->buf, "Date: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n",
                days[tm.tm_wday], tm.tm_mday, months[tm.tm_mon],
                tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
}

void httpd_put_date_hdr(outbuf_t *ob, const char *hdr, time_t now)
{
    struct tm tm;

    gmtime_r(&now, &tm);
    ob_addf(ob, "%s: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n",
            hdr, days[tm.tm_wday], tm.tm_mday, months[tm.tm_mon],
            tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/* rfc 2616: §4.2: Message Headers */

/* FIXME: deal with quotes and similar stuff in 'ps' */
static ALWAYS_INLINE bool http_hdr_equals(pstream_t ps, const char *v)
{
    size_t vlen = strlen(v);

    if (ps_len(&ps) != vlen)
        return false;
    for (size_t i = 0; i < vlen; i++) {
        if (tolower(ps.b[i]) != v[i])
            return false;
    }
    return true;
}

static bool http_hdr_contains(pstream_t ps, const char *v)
{
    pstream_t tmp;

    while (ps_get_ps_chr(&ps, ',', &tmp) == 0) {
        ps_trim(&tmp);
        __ps_skip(&ps, 1);

        if (http_hdr_equals(tmp, v))
            return true;
    }
    ps_trim(&ps);
    return http_hdr_equals(ps, v);
}

/* rfc 2616: §5.1: Request Line */

static int t_urldecode(httpd_qinfo_t *rq, pstream_t ps)
{
    char *buf  = t_new_raw(char, ps_len(&ps) + 1);
    char *p    = buf;
    char *vars = NULL;

    while (!ps_done(&ps)) {
        int c = __ps_getc(&ps);

        if (c == '+') {
            c = ' ';
        } else
        if (c == '%') {
            c = RETHROW(ps_hexdecode(&ps));
        }
        if (c == '?') {
            *p++ = '\0';
            vars = p;
            continue;
        }
        *p++ = c;
    }
    *p++ = '\0';

    path_simplify2(buf, true);
    rq->prefix = ps_initptr(NULL, NULL);
    rq->query  = ps_initstr(buf);
    if (vars) {
        rq->vars = ps_initptr(vars, p - 1);
    } else {
        rq->vars = ps_initptr(NULL, NULL);
    }
    return 0;
}

static int ps_get_ver(pstream_t *ps)
{
    int i = ps_geti(ps);

    PS_WANT(i >= 0 && i < 128);
    return i;
}

static int t_http_parse_request_line(pstream_t *ps, httpd_qinfo_t *req)
{
    pstream_t line, method, uri;

    PARSE_RETHROW(http_getline(ps, &line));

    PS_CHECK(ps_get_ps_chr(&line, ' ', &method));
    __ps_skip(&line, 1);

    switch (http_get_token_ps(method)) {
#define CASE(c)  case HTTP_TK_##c: req->method = HTTP_METHOD_##c; break
        CASE(CONNECT);
        CASE(DELETE);
        CASE(GET);
        CASE(HEAD);
        CASE(OPTIONS);
        CASE(POST);
        CASE(PUT);
        CASE(TRACE);
      default:
        req->method = HTTP_METHOD_ERROR;
        break;
#undef  CASE
    }

    PS_CHECK(ps_get_ps_chr(&line, ' ', &uri));
    __ps_skip(&line, 1);

    if (ps_skipstr(&uri, "http://") == 0 || ps_skipstr(&uri, "https://") == 0) {
        PS_CHECK(ps_get_ps_chr(&uri, '/', &req->host));
    } else {
        p_clear(&req->host, 1);
        if (uri.s[0] != '/' && !ps_memequal(&uri, "*", 1))
            return PARSE_ERROR;
    }
    RETHROW(t_urldecode(req, uri));
    PS_CHECK(ps_skipstr(&line, "HTTP/"));
    if (ps_len(&line) == 0 || !isdigit(line.b[0]))
        return PARSE_ERROR;
    req->http_version  = RETHROW(ps_get_ver(&line)) << 8;
    if (ps_getc(&line) != '.' || ps_len(&line) == 0 || !isdigit(line.b[0]))
        return PARSE_ERROR;
    req->http_version |= RETHROW(ps_get_ver(&line));
    return ps_len(&line) ? PARSE_ERROR : 0;
}

/* rfc 2616: §6.1: Status Line */

static inline int http_parse_status_line(pstream_t *ps, httpc_qinfo_t *qi)
{
    pstream_t line, code;

    PARSE_RETHROW(http_getline(ps, &line));

    if (ps_skipstr(&line, "HTTP/"))
        return PARSE_ERROR;
    if (ps_len(&line) == 0 || !isdigit(line.b[0]))
        return PARSE_ERROR;
    qi->http_version  = RETHROW(ps_get_ver(&line)) << 8;
    if (ps_getc(&line) != '.' || ps_len(&line) == 0 || !isdigit(line.b[0]))
        return PARSE_ERROR;
    qi->http_version |= RETHROW(ps_get_ver(&line));
    __ps_skip(&line, 1);

    if (ps_get_ps_chr(&line, ' ', &code) || ps_len(&code) != 3)
        return PARSE_ERROR;
    __ps_skip(&line, 1);

    qi->code = ps_geti(&code);
    if (qi->code < 100 || qi->code >= 600)
        return PARSE_ERROR;
    qi->reason = line;
    return PARSE_OK;
}

#undef PARSE_RETHROW

static void http_chunk_patch(outbuf_t *ob, char *buf, unsigned len)
{
    if (len == 0) {
        sb_shrink(&ob->sb, 12);
        ob->length      -= 12;
        ob->sb_trailing -= 12;
    } else {
        buf[0] = '\r';
        buf[1] = '\n';
        buf[2] = __str_digits_lower[(len >> 28) & 0xf];
        buf[3] = __str_digits_lower[(len >> 24) & 0xf];
        buf[4] = __str_digits_lower[(len >> 20) & 0xf];
        buf[5] = __str_digits_lower[(len >> 16) & 0xf];
        buf[6] = __str_digits_lower[(len >> 12) & 0xf];
        buf[7] = __str_digits_lower[(len >>  8) & 0xf];
        buf[8] = __str_digits_lower[(len >>  4) & 0xf];
        buf[9] = __str_digits_lower[(len >>  0) & 0xf];
        buf[10] = '\r';
        buf[11] = '\n';
    }
}

#define CLENGTH_RESERVE  12

static void
http_clength_patch(outbuf_t *ob, char s[static CLENGTH_RESERVE], unsigned len)
{
    (sprintf)(s, "%10d\r", len);
    s[CLENGTH_RESERVE - 1] = '\n';
}

/* }}} */
/* HTTPD Queries {{{ */

/*
 * HTTPD queries refcounting holds:
 *  - 1 for the fact that it has an owner.
 *  - 1 for the fact it hasn't been answered yet.
 *  - 1 for the fact it hasn't been parsed yet.
 * Hence it's obj_retained() on creation, always.
 */

httpd_qinfo_t *httpd_qinfo_dup(const httpd_qinfo_t *info)
{
    httpd_qinfo_t *res;
    size_t len = sizeof(info);
    void *p;
    intptr_t offs;

    len += sizeof(info->hdrs[0]) * info->hdrs_len;
    len += ps_len(&info->host);
    len += ps_len(&info->prefix);
    len += ps_len(&info->query);
    len += ps_len(&info->vars);
    len += ps_len(&info->hdrs_ps);

    res  = p_new_extra(httpd_qinfo_t, len);
    memcpy(res, info, offsetof(httpd_qinfo_t, host));
    res->hdrs          = (void *)&res[1];
    p                  = res->hdrs + res->hdrs_len;
    res->host.s        = p;
    res->host.s_end    = p = mempcpy(p, info->host.s, ps_len(&info->host));
    res->prefix.s      = p;
    res->prefix.s_end  = p = mempcpy(p, info->prefix.s, ps_len(&info->prefix));
    res->query.s       = p;
    res->query.s_end   = p = mempcpy(p, info->query.s, ps_len(&info->query));
    res->vars.s        = p;
    res->vars.s_end    = p = mempcpy(p, info->vars.s, ps_len(&info->vars));
    res->hdrs_ps.s     = p;
    res->hdrs_ps.s_end = mempcpy(p, info->hdrs_ps.s, ps_len(&info->hdrs_ps));

    offs = res->hdrs_ps.s - info->hdrs_ps.s;
    for (int i = 0; i < res->hdrs_len; i++) {
        http_qhdr_t       *lhs = &res->hdrs[i];
        const http_qhdr_t *rhs = &info->hdrs[i];

        lhs->wkhdr = rhs->wkhdr;
        lhs->key   = ps_initptr(rhs->key.s + offs, rhs->key.s_end + offs);
        lhs->val   = ps_initptr(rhs->val.s + offs, rhs->val.s_end + offs);
    }
    return res;
}

static httpd_query_t *httpd_query_create(httpd_t *w, httpd_trigger_t *cb)
{
    httpd_query_t *q;

    if (cb) {
        q = obj_new_of_class(httpd_query, cb->query_cls);
    } else {
        q = obj_new(httpd_query);
    }

    if (w->queries == 0)
        q->ob = &w->ob;
    /* ensure refcount is 3: owned, unanwsered, unparsed */
    obj_retain(q);
    obj_retain(q);
    q->owner = w;
    w->queries++;
    dlist_add_tail(&w->query_list, &q->query_link);
    if (cb)
        q->trig_cb = httpd_trigger_dup(cb);
    return q;
}

static ALWAYS_INLINE void httpd_query_detach(httpd_query_t *q)
{
    httpd_t *w = q->owner;

    if (w) {
        if (!q->own_ob)
            q->ob = NULL;
        dlist_remove(&q->query_link);
        w->queries--;
        w->queries_done -= q->answered;
        q->owner = NULL;
        obj_release(q);
    }
}

static httpd_query_t *httpd_query_init(httpd_query_t *q)
{
    sb_init(&q->payload);
    q->http_version = HTTP_1_1;
    return q;
}

static void httpd_query_wipe(httpd_query_t *q)
{
    if (q->trig_cb) {
        if (q->trig_cb->on_query_wipe)
            q->trig_cb->on_query_wipe(q);
        httpd_trigger_delete(&q->trig_cb);
    }
    if (q->own_ob)
        ob_delete(&q->ob);
    httpd_qinfo_delete(&q->qinfo);
    sb_wipe(&q->payload);
    httpd_query_detach(q);
}

static void httpd_query_on_data_bufferize(httpd_query_t *q, pstream_t ps)
{
    size_t plen = ps_len(&ps);

    if (unlikely(plen + q->payload.len > q->payload_max_size)) {
        httpd_reject(q, REQUEST_ENTITY_TOO_LARGE,
                     "payload is larger than %d octets",
                     q->payload_max_size);
        return;
    }
    sb_add(&q->payload, ps.s, plen);
}

void httpd_bufferize(httpd_query_t *q, unsigned maxsize)
{
    const httpd_qinfo_t *inf = q->qinfo;

    q->payload_max_size = maxsize;
    q->on_data          = &httpd_query_on_data_bufferize;
    if (!inf)
        return;
    for (int i = inf->hdrs_len; i-- > 0; ) {
        if (inf->hdrs[i].wkhdr == HTTP_WKHDR_CONTENT_LENGTH) {
            uint64_t len = strtoull(inf->hdrs[i].val.s, NULL, 0);

            if (unlikely(len > maxsize)) {
                httpd_reject(q, REQUEST_ENTITY_TOO_LARGE,
                             "payload is larger than %d octets", maxsize);
            } else {
                sb_grow(&q->payload, len);
            }
            return;
        }
    }
}

OBJ_VTABLE(httpd_query)
    httpd_query.init     = httpd_query_init;
    httpd_query.wipe     = httpd_query_wipe;
OBJ_VTABLE_END()


/*---- low level httpd_query reply functions ----*/

outbuf_t *httpd_reply_hdrs_start(httpd_query_t *q, int code, bool force_uncacheable)
{
    outbuf_t *ob = httpd_get_ob(q);

    http_update_date_cache(&date_cache_g, lp_getsec());

    assert (!q->hdrs_started && !q->hdrs_done);

    q->answer_code = code;
    ob_addf(ob, "HTTP/1.%d %d %*pM\r\n", HTTP_MINOR(q->http_version),
            code, LSTR_FMT_ARG(http_code_to_str(code)));
    ob_add(ob, date_cache_g.buf, sizeof(date_cache_g.buf) - 1);
    if (q->owner && q->owner->connection_close) {
        if (!q->conn_close) {
            ob_adds(ob, "Connection: close\r\n");
            q->conn_close = true;
        }
    }
    if (force_uncacheable) {
        ob_adds(ob,
                "Cache-Control: no-store, no-cache, must-revalidate\r\n"
                "Pragma: no-cache\r\n");
    }
    q->hdrs_started = true;
    return ob;
}

void httpd_reply_hdrs_done(httpd_query_t *q, int clen, bool chunked)
{
    outbuf_t *ob = httpd_get_ob(q);

    assert (!q->hdrs_done);
    q->hdrs_done = true;

    if (clen >= 0) {
        ob_addf(ob, "Content-Length: %d\r\n\r\n", clen);
        return;
    }

    if (chunked) {
        if (likely(q->http_version != HTTP_1_0)) {
            q->chunked = true;
            ob_adds(ob, "Transfer-Encoding: chunked\r\n");
            /* XXX: no \r\n because http_chunk_patch adds it */
        } else {
            /* FIXME: we aren't allowed to fallback to the non chunked case
             *        here because it would break assumptions from the caller
             *        that it can stream the answer with returns in the event
             *        loop
             */
            if (!q->conn_close) {
                ob_adds(ob, "Connection: close\r\n");
                q->conn_close = true;
            }
            if (q->owner)
                q->owner->connection_close = true;
            ob_adds(ob, "\r\n");
        }
    } else {
        q->clength_hack = true;
        ob_adds(ob, "Content-Length: ");
        q->chunk_hdr_offs    = ob_reserve(ob, CLENGTH_RESERVE);
        ob_adds(ob, "\r\n");
        q->chunk_prev_length = ob->length;
    }
}

void httpd_reply_chunk_done_(httpd_query_t *q, outbuf_t *ob)
{
    assert (q->chunk_started);
    q->chunk_started = false;
    http_chunk_patch(ob, ob->sb.data + q->chunk_hdr_offs,
                     ob->length - q->chunk_prev_length);
}

void httpd_reply_done(httpd_query_t *q)
{
    outbuf_t *ob = httpd_get_ob(q);

    assert (q->hdrs_done && !q->answered && !q->chunk_started);
    if (q->chunked)
        ob_adds(ob, "\r\n0\r\n\r\n");
    if (q->clength_hack) {
        http_clength_patch(ob, ob->sb.data + q->chunk_hdr_offs,
                           ob->length - q->chunk_prev_length);
        q->clength_hack = false;
    }
    httpd_mark_query_answered(q);
}

static void httpd_set_mask(httpd_t *w);

void httpd_signal_write(httpd_query_t *q)
{
    httpd_t *w = q->owner;

    if (w) {
        assert (q->hdrs_done && !q->answered && !q->chunk_started);
        httpd_set_mask(w);
    }
}

/*---- high level httpd_query reply functions ----*/

static ALWAYS_INLINE void httpd_query_reply_100continue_(httpd_query_t *q)
{
    if (q->answered || q->hdrs_started)
        return;
    if (q->expect100cont) {
        ob_addf(httpd_get_ob(q), "HTTP/1.%d 100 Continue\r\n\r\n",
                HTTP_MINOR(q->http_version));
        q->expect100cont = false;
    }
}

void httpd_reply_100continue(httpd_query_t *q)
{
    httpd_query_reply_100continue_(q);
}

void httpd_reply_202accepted(httpd_query_t *q)
{
    if (q->answered || q->hdrs_started)
        return;

    httpd_reply_hdrs_start(q, HTTP_CODE_ACCEPTED, false);
    httpd_reply_hdrs_done(q, 0, false);
    httpd_reply_done(q);
}

void httpd_reject_(httpd_query_t *q, int code, const char *fmt, ...)
{
    va_list ap;
    outbuf_t *ob;

    if (q->answered || q->hdrs_started)
        return;

    ob = httpd_reply_hdrs_start(q, code, false);
    ob_adds(ob, "Content-Type: text/html\r\n");
    httpd_reply_hdrs_done(q, -1, false);

    ob_addf(ob, "<html><body><h1>%d - %*pM</h1><p>",
            code, LSTR_FMT_ARG(http_code_to_str(code)));
    va_start(ap, fmt);
    ob_addvf(ob, fmt, ap);
    va_end(ap);
    ob_adds(ob, "</p></body></html>\r\n");
    httpd_reply_done(q);
}

void httpd_reject_unauthorized(httpd_query_t *q, lstr_t auth_realm)
{
    const lstr_t body = LSTR_IMMED_V("<html><body>"
                                     "<h1>401 - Authentication required</h1>"
                                     "</body></html>\r\n");
    outbuf_t *ob;

    if (q->answered || q->hdrs_started)
        return;

    ob = httpd_reply_hdrs_start(q, HTTP_CODE_UNAUTHORIZED, false);
    ob_adds(ob, "Content-Type: text/html\r\n");
    ob_addf(ob, "WWW-Authenticate: Basic realm=\"%*pM\"\r\n",
            LSTR_FMT_ARG(auth_realm));
    httpd_reply_hdrs_done(q, body.len, false);
    ob_add(ob, body.s, body.len);
    httpd_reply_done(q);
}

/* }}} */
/* HTTPD Triggers {{{ */

static httpd_trigger_node_t *
httpd_trigger_node_new(httpd_trigger_node_t *parent, lstr_t path)
{
    httpd_trigger_node_t *node;
    int pos;

    pos = __qm_put(http_path, &parent->childs, &path, NULL, 0);
    if (pos & QHASH_COLLISION)
        return parent->childs.values[pos & ~QHASH_COLLISION];

    parent->childs.values[pos] = node =
        p_new_extra(httpd_trigger_node_t, path.len + 1);
    qm_init(http_path, &node->childs, true);
    memcpy(node->path, path.s, path.len + 1);
    return node;
}

static void httpd_trigger_node_wipe(httpd_trigger_node_t *node);
GENERIC_DELETE(httpd_trigger_node_t, httpd_trigger_node);

static void httpd_trigger_node_wipe(httpd_trigger_node_t *node)
{
    httpd_trigger_delete(&node->cb);
    qm_for_each_pos(http_path, pos, &node->childs) {
        httpd_trigger_node_delete(&node->childs.values[pos]);
        qm_del_at(http_path, &node->childs, pos);
    }
    qm_wipe(http_path, &node->childs);
}

void httpd_trigger_register_(httpd_trigger_node_t *n, const char *path,
                             httpd_trigger_t *cb)
{
    while (*path == '/')
        path++;
    while (*path) {
        const char  *q = strchrnul(path, '/');
        lstr_t s = LSTR_INIT(path, q - path);

        n = httpd_trigger_node_new(n, s);
        while (*q == '/')
            q++;
        path = q;
    }
    httpd_trigger_delete(&n->cb);
    n->cb = httpd_trigger_dup(cb);
    if (unlikely(cb->query_cls == NULL))
        cb->query_cls = obj_class(httpd_query);
}

static bool httpd_trigger_unregister__(httpd_trigger_node_t *n, const char *path)
{
    while (*path == '/')
        path++;

    if (!*path) {
        httpd_trigger_delete(&n->cb);
    } else {
        const char *q = strchrnul(path, '/');
        lstr_t      s = LSTR_INIT(path, q - path);
        int pos       = qm_find(http_path, &n->childs, &s);

        if (pos < 0)
            return false;
        if (httpd_trigger_unregister__(n->childs.values[pos], q)) {
            httpd_trigger_node_delete(&n->childs.values[pos]);
            qm_del_at(http_path, &n->childs, pos);
        }
    }
    return qm_len(http_path, &n->childs) == 0;
}

void httpd_trigger_unregister_(httpd_trigger_node_t *n, const char *path)
{
    httpd_trigger_unregister__(n, path);
}

/* XXX: assumes path is canonical wrt '/' and starts with one */
static httpd_trigger_t *
httpd_trigger_resolve(httpd_trigger_node_t *n, httpd_qinfo_t *req)
{
    httpd_trigger_t *res = n->cb;
    const char *p = req->query.s;
    const char *q = req->query.s_end;

    req->prefix = ps_initptr(p, p);
    while (p++ < q) {
        lstr_t s;
        int pos;

        s.s   = p;
        p     = memchr(p, '/', q - p) ?: q;
        s.len = p - s.s;
        pos   = qm_find(http_path, &n->childs, &s);
        if (pos < 0)
            break;
        n = n->childs.values[pos];
        if (n->cb) {
            res = n->cb;
            req->query.s = req->prefix.s_end = p;
        }
    }
    return res;
}

/* }}} */
/* HTTPD Parser {{{ */

static void httpd_set_mask(httpd_t *w)
{
    int mask = POLLIN;

    if (unlikely(w->queries >= w->cfg->pipeline_depth))
        mask = 0;
    if (unlikely(w->ob.length >= (int)w->cfg->outbuf_max_size))
        mask = 0;
    if (unlikely(w->state == HTTP_PARSER_CLOSE))
        mask = 0;
    if (!ob_is_empty(&w->ob))
        mask |= POLLOUT;
    el_fd_set_mask(w->ev, mask);
}

static void httpd_flush_answered(httpd_t *w)
{
    httpd_query_t *q;

    dlist_for_each_entry_safe(q, &w->query_list, query_link) {
        if (q->own_ob) {
            ob_merge_delete(&w->ob, &q->ob);
            q->own_ob = false;
        }
        if (!q->answered) {
            q->ob = &w->ob;
            break;
        }
        if (likely(q->parsed))
            httpd_query_detach(q);
    }
    httpd_set_mask(w);
}

static void httpd_query_done(httpd_t *w, httpd_query_t *q)
{
    struct timeval now;

    lp_gettv(&now);
    q->query_sec  = now.tv_sec;
    q->query_usec = now.tv_usec;
    q->parsed     = true;
    httpd_flush_answered(w);
    if (w->connection_close) {
        w->state = HTTP_PARSER_CLOSE;
    } else {
        w->state = HTTP_PARSER_IDLE;
    }
    w->chunk_length = 0;
    obj_release(q);
}

static void httpd_mark_query_answered(httpd_query_t *q)
{
    assert (!q->answered);
    q->answered = true;
    q->on_data  = NULL;
    q->on_done  = NULL;
    q->on_ready = NULL;
    if (q->owner) {
        httpd_t *w = q->owner;

        w->queries_done++;
        if (dlist_is_first(&w->query_list, &q->query_link))
            httpd_flush_answered(w);
    }
    q->expect100cont = false;
    obj_release(q);
}

qvector_t(qhdr, http_qhdr_t);
static void httpd_do_any(httpd_t *w, httpd_query_t *q, httpd_qinfo_t *req);
static void httpd_do_trace(httpd_t *w, httpd_query_t *q, httpd_qinfo_t *req);

static int httpd_parse_idle(httpd_t *w, pstream_t *ps)
{
    t_scope;
    size_t start = w->chunk_length > 4 ? w->chunk_length - 4 : 0;
    httpd_qinfo_t req;
    const uint8_t *p;
    pstream_t buf;
    int clen = -1;
    bool chunked = false;
    httpd_query_t *q;
    qv_t(qhdr) hdrs;
    httpd_trigger_t *cb = NULL;
    struct timeval now;

    if ((p = memmem(ps->s + start, ps_len(ps) - start, "\r\n\r\n", 4)) == NULL) {
        if (ps_len(ps) > _G.header_size_max) {
            q = httpd_query_create(w, NULL);
            httpd_reject(q, FORBIDDEN, "Headers exceed %d octets",
                         _G.header_size_max);
            goto unrecoverable_error;
        }
        w->chunk_length = ps_len(ps);
        return PARSE_MISSING_DATA;
    }

    if (--w->max_queries == 0)
        w->connection_close = true;

    req.hdrs_ps = ps_initptr(ps->s, p + 4);
    if (t_http_parse_request_line(ps, &req) < 0) {
        q = httpd_query_create(w, NULL);
        httpd_reject(q, BAD_REQUEST, "Invalid request line");
        goto unrecoverable_error;
    }

    if ((unsigned)req.method < countof(w->cfg->roots))
        cb = httpd_trigger_resolve(&w->cfg->roots[req.method], &req);
    q = httpd_query_create(w, cb);
    q->http_version = req.http_version;
    buf = __ps_get_ps_upto(ps, p + 2);
    __ps_skip_upto(ps, p + 4);
    switch (req.http_version) {
      case HTTP_1_0:
        /* TODO: support old-style Keep-Alive ? */
        w->connection_close = true;
        break;
      case HTTP_1_1:
        break;
      default:
        httpd_reject(q, NOT_IMPLEMENTED,
                     "This server requires an HTTP/1.1 compatible client");
        goto unrecoverable_error;
    }

    lp_gettv(&now);
    q->query_sec  = now.tv_sec;
    q->query_usec = now.tv_usec;
    t_qv_init(qhdr, &hdrs, 64);

    while (!ps_done(&buf)) {
        http_qhdr_t *qhdr = qv_growlen(qhdr, &hdrs, 1);

        /* TODO: normalize, make "lists" */
        qhdr->key = ps_get_cspan(&buf, &http_non_token);
        if (ps_len(&qhdr->key) == 0 || __ps_getc(&buf) != ':') {
            httpd_reject(q, BAD_REQUEST,
                         "Header name is empty or not followed by a colon");
            goto unrecoverable_error;
        }
        qhdr->val.s = buf.s;
        for (;;) {
            ps_skip_afterchr(&buf, '\r');
            if (__ps_getc(&buf) != '\n') {
                httpd_reject(q, BAD_REQUEST,
                             "CR is not followed by a LF in headers");
                goto unrecoverable_error;
            }
            qhdr->val.s_end = buf.s - 2;
            if (ps_done(&buf))
                break;
            if (buf.s[0] != '\t' && buf.s[0] != ' ')
                break;
            __ps_skip(&buf, 1);
        }
        ps_trim(&qhdr->val);

        switch ((qhdr->wkhdr = http_wkhdr_from_ps(qhdr->key))) {
          case HTTP_WKHDR_HOST:
            if (ps_len(&req.host) == 0)
                req.host = qhdr->val;
            qv_shrink(qhdr, &hdrs, 1);
            break;

          case HTTP_WKHDR_EXPECT:
            q->expect100cont |= http_hdr_equals(qhdr->key, "100-continue");
            break;

          case HTTP_WKHDR_CONNECTION:
            w->connection_close |= http_hdr_contains(qhdr->val, "close");
            break;

          case HTTP_WKHDR_TRANSFER_ENCODING:
            /* rfc 2616: §4.4: != "identity" means chunked encoding */
            switch (http_get_token_ps(qhdr->val)) {
              case HTTP_TK_IDENTITY:
                chunked = false;
                break;
              case HTTP_TK_CHUNKED:
                chunked = true;
                break;
              default:
                httpd_reject(q, NOT_IMPLEMENTED,
                             "Transfer-Encoding %*pM is unimplemented",
                             (int)ps_len(&qhdr->val), qhdr->val.s);
                break;
            }
            break;

          case HTTP_WKHDR_CONTENT_LENGTH:
            clen = memtoip(qhdr->val.b, ps_len(&qhdr->val), &p);
            if (p != qhdr->val.b_end) {
                httpd_reject(q, BAD_REQUEST, "Content-Length is unparseable");
                goto unrecoverable_error;
            }
            break;

          default:
            break;
        }
    }

    if (chunked) {
        /* rfc 2616: §4.4: if chunked, then ignore any Content-Length */
        w->chunk_length = clen = 0;
        w->state = HTTP_PARSER_CHUNK_HDR;
    } else {
        w->chunk_length = clen < 0 ? 0 : clen;
        w->state = HTTP_PARSER_BODY;
    }
    req.hdrs      = hdrs.tab;
    req.hdrs_len  = hdrs.len;

    switch (req.method) {
      case HTTP_METHOD_TRACE:
        httpd_do_trace(w, q, &req);
        break;
      case HTTP_METHOD_POST:
      case HTTP_METHOD_PUT:
        if (clen < 0) {
            httpd_reject(q, LENGTH_REQUIRED, "");
            goto unrecoverable_error;
        }
        /* FALLTHROUGH */
      default:
        httpd_do_any(w, q, &req);
        break;
    }
    httpd_query_reply_100continue_(q);
    return PARSE_OK;

  unrecoverable_error:
    w->connection_close = true;
    httpd_query_done(w, q);
    return PARSE_OK;
}

static int httpd_parse_body(httpd_t *w, pstream_t *ps)
{
    httpd_query_t *q = dlist_last_entry(&w->query_list, httpd_query_t, query_link);
    size_t plen = ps_len(ps);

    q->expect100cont = false;
    if (plen >= w->chunk_length) {
        pstream_t tmp = __ps_get_ps(ps, w->chunk_length);

        if (q->on_data)
            q->on_data(q, tmp);
        if (q->on_done)
            q->on_done(q);
        httpd_query_done(w, q);
        return PARSE_OK;
    }

    if (plen >= w->cfg->on_data_threshold) {
        if (q->on_data)
            q->on_data(q, *ps);
        w->chunk_length -= plen;
        ps->b = ps->b_end;
    }
    return PARSE_MISSING_DATA;
}

/*
 * rfc 2616: §3.6.1: Chunked Transfer Coding
 *
 * - All chunked extensions are stripped (support is optionnal)
 * - trailer headers are ignored, as:
 *   + Clients must specifically ask for them (we won't)
 *   + or ignoring them should not modify behaviour (so we do ignore them).
 */
static int httpd_parse_chunk_hdr(httpd_t *w, pstream_t *ps)
{
    httpd_query_t *q = dlist_last_entry(&w->query_list, httpd_query_t, query_link);
    pstream_t line, hex;
    uint64_t  len = 0;
    int res;

    q->expect100cont = false;
    res = http_getline(ps, &line);
    if (res > 0)
        return res;
    if (res < 0)
        goto cancel_query;
    http_skipspaces(&line);
    hex = ps_get_span(&line, &ctype_ishexdigit);
    http_skipspaces(&line);
    if (unlikely(ps_len(&line)) != 0 && unlikely(line.s[0] != ';'))
        goto cancel_query;
    if (unlikely(ps_len(&hex) == 0) || unlikely(ps_len(&hex) > 16))
        goto cancel_query;
    for (const char *s = hex.s; s < hex.s_end; s++)
        len = (len << 4) | __str_digit_value[*s + 128];
    w->chunk_length = len;
    w->state = len ? HTTP_PARSER_CHUNK : HTTP_PARSER_CHUNK_TRAILER;
    return PARSE_OK;

  cancel_query:
    httpd_reject(q, BAD_REQUEST, "Chunked header is unparseable");
    w->connection_close = true;
    httpd_query_done(w, q);
    return PARSE_OK;
}

static int httpd_parse_chunk(httpd_t *w, pstream_t *ps)
{
    httpd_query_t *q = dlist_last_entry(&w->query_list, httpd_query_t, query_link);
    size_t plen = ps_len(ps);

    if (plen >= w->chunk_length + 2) {
        pstream_t tmp = __ps_get_ps(ps, w->chunk_length);

        if (ps_skipstr(ps, "\r\n")) {
            httpd_reject(q, BAD_REQUEST, "Chunked header is unparseable");
            w->connection_close = true;
            httpd_query_done(w, q);
            return PARSE_ERROR;
        }
        if (q->on_data)
            q->on_data(q, tmp);
        w->state = HTTP_PARSER_CHUNK_HDR;
        return PARSE_OK;
    }
    if (plen >= w->cfg->on_data_threshold) {
        if (q->on_data)
            q->on_data(q, *ps);
        w->chunk_length -= plen;
        ps->b = ps->b_end;
    }
    return PARSE_MISSING_DATA;
}

static int httpd_parse_chunk_trailer(httpd_t *w, pstream_t *ps)
{
    httpd_query_t *q = dlist_last_entry(&w->query_list, httpd_query_t, query_link);
    pstream_t line;

    do {
        int res = (http_getline(ps, &line));

        if (res < 0) {
            httpd_reject(q, BAD_REQUEST, "Trailer headers are unparseable");
            w->connection_close = true;
            httpd_query_done(w, q);
            return PARSE_OK;
        }
        if (res > 0)
            return res;
    } while (ps_len(&line));
    if (q->on_done)
        q->on_done(q);
    httpd_query_done(w, q);
    return PARSE_OK;
}

static int httpd_parse_close(httpd_t *w, pstream_t *ps)
{
    ps->b = ps->b_end;
    return PARSE_MISSING_DATA;
}


static int (*httpd_parsers[])(httpd_t *w, pstream_t *ps) = {
    [HTTP_PARSER_IDLE]          = httpd_parse_idle,
    [HTTP_PARSER_BODY]          = httpd_parse_body,
    [HTTP_PARSER_CHUNK_HDR]     = httpd_parse_chunk_hdr,
    [HTTP_PARSER_CHUNK]         = httpd_parse_chunk,
    [HTTP_PARSER_CHUNK_TRAILER] = httpd_parse_chunk_trailer,
    [HTTP_PARSER_CLOSE]         = httpd_parse_close,
};

/* }}} */
/* HTTPD {{{ */

httpd_cfg_t *httpd_cfg_init(httpd_cfg_t *cfg)
{
    p_clear(cfg, 1);

    dlist_init(&cfg->httpd_list);
    cfg->httpd_cls         = obj_class(httpd);
    cfg->outbuf_max_size   = _G.outbuf_max_size;
    cfg->on_data_threshold = _G.on_data_threshold;
    cfg->pipeline_depth    = _G.pipeline_depth_in;
    cfg->noact_delay       = _G.noact_delay;
    cfg->max_queries       = _G.max_queries_in;
    cfg->max_conns         = _G.max_conns_in;
    for (int i = 0; i < countof(cfg->roots); i++) {
        qm_init(http_path, &cfg->roots[i].childs, true);
    }
    return cfg;
}

void httpd_cfg_wipe(httpd_cfg_t *cfg)
{
    for (int i = 0; i < countof(cfg->roots); i++) {
        httpd_trigger_node_wipe(&cfg->roots[i]);
    }
    assert (dlist_is_empty(&cfg->httpd_list));
}

static httpd_t *httpd_init(httpd_t *w)
{
    dlist_init(&w->query_list);
    dlist_init(&w->httpd_link);
    sb_init(&w->ibuf);
    ob_init(&w->ob);
    w->state = HTTP_PARSER_IDLE;
    return w;
}

static void httpd_wipe(httpd_t *w)
{
    el_fd_unregister(&w->ev, true);
    sb_wipe(&w->ibuf);
    ob_wipe(&w->ob);
    dlist_for_each_safe(it, &w->query_list) {
        httpd_query_detach(dlist_entry(it, httpd_query_t, query_link));
    }
    w->cfg->nb_conns--;
    dlist_remove(&w->httpd_link);
    httpd_cfg_delete(&w->cfg);
}

OBJ_VTABLE(httpd)
    httpd.init = httpd_init,
    httpd.wipe = httpd_wipe,
OBJ_VTABLE_END()

void httpd_close_gently(httpd_t *w)
{
    w->connection_close = true;
    if (w->state == HTTP_PARSER_IDLE) {
        w->state = HTTP_PARSER_CLOSE;
        /* let the event loop maybe destroy us later, not now */
        el_fd_set_mask(w->ev, POLLOUT);
    }
}

int t_httpd_qinfo_get_basic_auth(const httpd_qinfo_t *info,
                                 pstream_t *user, pstream_t *pw)
{
    for (int i = info->hdrs_len; i-- > 0; ) {
        const http_qhdr_t *hdr = info->hdrs + i;
        pstream_t v;
        char *colon;
        sb_t sb;
        int len;

        if (hdr->wkhdr != HTTP_WKHDR_AUTHORIZATION)
            continue;
        v = hdr->val;
        ps_skipspaces(&v);
        PS_CHECK(ps_skipcasestr(&v, "basic"));
        ps_trim(&v);

        len = ps_len(&v);
        t_sb_init(&sb, len + 1);
        PS_CHECK(sb_add_unb64(&sb, v.s, len));
        colon = strchr(sb.data, ':');
        if (!colon)
            return -1;
        *user    = ps_initptr(sb.data, colon);
        *colon++ = '\0';
        *pw      = ps_initptr(colon, sb_end(&sb));
        return 0;
    }

    *pw = *user = ps_initptr(NULL, NULL);
    return 0;
}

static void httpd_do_any(httpd_t *w, httpd_query_t *q, httpd_qinfo_t *req)
{
    httpd_trigger_t *cb = q->trig_cb;
    pstream_t user, pw;

    if (ps_memequal(&req->query, "*", 1)) {
        httpd_reject(q, NOT_FOUND, "'*' not found");
        return;
    }

    if (cb) {
        if (cb->auth) {
            t_scope;

            if (unlikely(t_httpd_qinfo_get_basic_auth(req, &user, &pw) < 0)) {
                httpd_reject(q, BAD_REQUEST, "invalid Authentication header");
                return;
            }
            (*cb->auth)(cb, q, user, pw);
        }
        if (likely(!q->answered)) {
            (*cb->cb)(cb, q, req);
        }
    } else {
        httpd_reject(q, NOT_IMPLEMENTED, "");
    }
}

static void httpd_do_trace_on_data(httpd_query_t *q, pstream_t ps)
{
    outbuf_t *ob = httpd_get_ob(q);
    size_t dlen = ps_len(&ps);

    if (dlen) {
        ob_addf(ob, "\r\n%zx\r\n", dlen);
        ob_add(ob, ps.s, dlen);
    }
}

static void httpd_do_trace(httpd_t *w, httpd_query_t *q, httpd_qinfo_t *req)
{
    outbuf_t *ob;

    if (q->http_version == HTTP_1_0) {
        httpd_reject(q, NOT_IMPLEMENTED, "TRACE on HTTP/1.0 isn't supported");
        return;
    }

    q->on_data = &httpd_do_trace_on_data;
    q->on_done = &httpd_reply_done;
    ob = httpd_reply_hdrs_start(q, HTTP_CODE_OK, false);
    ob_adds(ob, "Content-Type: message/http\r\n");
    httpd_reply_hdrs_done(q, -1, true);
    ob_addf(ob, "\r\n%zx\r\n", ps_len(&req->hdrs_ps));
    ob_add(ob, req->hdrs_ps.s, ps_len(&req->hdrs_ps));
}

static int httpd_on_event(el_t evh, int fd, short events, el_data_t priv)
{
    httpd_t *w = priv.ptr;
    pstream_t ps;
    int res;

    if (events == EL_EVENTS_NOACT)
        goto close;

    if (events & POLLIN) {
        if (sb_read(&w->ibuf, fd, 0) <= 0)
            goto close;

        ps = ps_initsb(&w->ibuf);
        do {
            res = (*httpd_parsers[w->state])(w, &ps);
        } while (res == PARSE_OK);
        sb_skip_upto(&w->ibuf, ps.s);
    }

    {
        int oldlen = w->ob.length;
        if (ob_write(&w->ob, fd) < 0 && !ERR_RW_RETRIABLE(errno))
            goto close;

        if (!dlist_is_empty(&w->query_list)) {
            httpd_query_t *query = dlist_first_entry(&w->query_list,
                                                     httpd_query_t, query_link);
            if (!query->answered && query->on_ready != NULL
            && oldlen >= query->ready_threshold
            && w->ob.length < query->ready_threshold) {
                (*query->on_ready)(query);
            }
        }
    }

    if (unlikely(w->state == HTTP_PARSER_CLOSE)) {
        if (w->queries == 0 && ob_is_empty(&w->ob))
            goto close;
    } else {
        /* w->state == HTTP_PARSER_IDLE:
         *   queries > 0 means pending answer, client isn't lagging, we are.
         *
         * w->state != HTTP_PARSER_IDLE:
         *   queries is always > 0: the query being parsed has been created.
         *   So for this case, pending requests without answers exist iff
         *   queries > 1.
         */
        if (w->queries > (w->state != HTTP_PARSER_IDLE)) {
            el_fd_watch_activity(w->ev, POLLINOUT, 0);
        } else
        if (ob_is_empty(&w->ob)) {
            el_fd_watch_activity(w->ev, POLLINOUT, w->cfg->noact_delay);
        }
    }
    httpd_set_mask(w);
    return 0;

  close:
    if (!dlist_is_empty(&w->query_list)) {
        httpd_query_t *q;

        q = dlist_last_entry(&w->query_list, httpd_query_t, query_link);
        if (!q->parsed)
            obj_release(q);
        if (!q->answered)
            obj_release(q);
    }
    obj_delete(&w);
    return 0;
}

static int
httpd_on_accept(el_t evh, int fd, short events, el_data_t priv)
{
    httpd_cfg_t *cfg = priv.ptr;
    int sock;

    while ((sock = acceptx(fd, O_NONBLOCK)) >= 0) {
        if (cfg->nb_conns >= cfg->max_conns) {
            close(sock);
        } else {
            httpd_spawn(sock, priv.ptr);
        }
    }
    return 0;
}

el_t httpd_listen(sockunion_t *su, httpd_cfg_t *cfg)
{
    int fd = listenx(-1, su, 1, SOCK_STREAM, IPPROTO_TCP, O_NONBLOCK);

    if (fd < 0)
        return NULL;
    return el_unref(el_fd_register(fd, POLLIN, httpd_on_accept,
                                   httpd_cfg_dup(cfg)));
}

void httpd_unlisten(el_t *ev)
{
    httpd_cfg_t *cfg = el_fd_unregister(ev, true).ptr;

    httpd_cfg_delete(&cfg);
}

httpd_t *httpd_spawn(int fd, httpd_cfg_t *cfg)
{
    httpd_t *w = obj_new_of_class(httpd, cfg->httpd_cls);

    cfg->nb_conns++;
    w->cfg         = httpd_cfg_dup(cfg);
    w->ev          = el_unref(el_fd_register(fd, POLLIN, &httpd_on_event, w));
    w->max_queries = cfg->max_queries;
    el_fd_watch_activity(w->ev, POLLINOUT, w->cfg->noact_delay);
    dlist_add_tail(&cfg->httpd_list, &w->httpd_link);
    return w;
}

/* }}} */
/* HTTPC Parsers {{{ */

static httpc_qinfo_t *httpc_qinfo_dup(const httpc_qinfo_t *info)
{
    httpc_qinfo_t *res;
    size_t len = sizeof(info);
    void *p;
    intptr_t offs;

    len += sizeof(info->hdrs[0]) * info->hdrs_len;
    len += ps_len(&info->reason);
    len += ps_len(&info->hdrs_ps);

    res  = p_new_extra(httpc_qinfo_t, len);
    memcpy(res, info, offsetof(httpc_qinfo_t, hdrs_ps));
    res->hdrs          = (void *)&res[1];
    p                  = res->hdrs + res->hdrs_len;
    res->reason.s      = p;
    res->reason.s_end  = p = mempcpy(p, info->reason.s, ps_len(&info->reason));
    res->hdrs_ps.s     = p;
    res->hdrs_ps.s_end = mempcpy(p, info->hdrs_ps.s, ps_len(&info->hdrs_ps));

    offs = res->hdrs_ps.s - info->hdrs_ps.s;
    for (int i = 0; i < res->hdrs_len; i++) {
        http_qhdr_t       *lhs = &res->hdrs[i];
        const http_qhdr_t *rhs = &info->hdrs[i];

        lhs->wkhdr = rhs->wkhdr;
        lhs->key   = ps_initptr(rhs->key.s + offs, rhs->key.s_end + offs);
        lhs->val   = ps_initptr(rhs->val.s + offs, rhs->val.s_end + offs);
    }
    return res;
}

static void httpc_query_on_done(httpc_query_t *q, int status)
{
    httpc_t *w = q->owner;

    if (w) {
        if (--w->queries < w->cfg->pipeline_depth && w->max_queries && w->busy)
            obj_vcall(w, set_ready, false);
        q->owner = NULL;
    }
    dlist_remove(&q->query_link);
    (*q->on_done)(q, status);
}
#define httpc_query_abort(q)  httpc_query_on_done(q, HTTPC_STATUS_ABORT)

static int httpc_query_ok(httpc_query_t *q)
{
    httpc_t *w = q->owner;

    httpc_query_on_done(q, HTTPC_STATUS_OK);
    if (w) {
        w->chunk_length = 0;
        w->state = HTTP_PARSER_IDLE;
    }
    return PARSE_OK;
}

static inline void httpc_qinfo_delete(httpc_qinfo_t **infop)
{
    p_delete(infop);
}

static int httpc_parse_idle(httpc_t *w, pstream_t *ps)
{
    t_scope;
    size_t start = w->chunk_length > 4 ? w->chunk_length - 4 : 0;
    httpc_qinfo_t req;
    const uint8_t *p;
    pstream_t buf;
    qv_t(qhdr) hdrs;
    httpc_query_t *q;
    bool chunked = false, conn_close = false;
    int clen = -1, res;

    if (ps_len(ps) > 0 && dlist_is_empty(&w->query_list)) {
        e_trace(0, "UHOH spurious data from the HTTP server: %*pM",
                (int)ps_len(ps), ps->s);
        return PARSE_ERROR;
    }

    if ((p = memmem(ps->s + start, ps_len(ps) - start, "\r\n\r\n", 4)) == NULL) {
        if (ps_len(ps) > _G.header_size_max)
            return PARSE_ERROR;
        w->chunk_length = ps_len(ps);
        return PARSE_MISSING_DATA;
    }

    req.hdrs_ps = ps_initptr(ps->s, p + 4);
    res = http_parse_status_line(ps, &req);
    if (res)
        return res;

    buf = __ps_get_ps_upto(ps, p + 2);
    __ps_skip_upto(ps, p + 4);
    t_qv_init(qhdr, &hdrs, 64);

    while (!ps_done(&buf)) {
        http_qhdr_t *qhdr = qv_growlen(qhdr, &hdrs, 1);

        /* TODO: normalize, make "lists" */
        qhdr->key = ps_get_cspan(&buf, &http_non_token);
        if (ps_len(&qhdr->key) == 0 || __ps_getc(&buf) != ':')
            return PARSE_ERROR;
        qhdr->val.s = buf.s;
        for (;;) {
            ps_skip_afterchr(&buf, '\r');
            if (__ps_getc(&buf) != '\n')
                return PARSE_ERROR;
            qhdr->val.s_end = buf.s - 2;
            if (ps_done(&buf))
                break;
            if (buf.s[0] != '\t' && buf.s[0] != ' ')
                break;
            __ps_skip(&buf, 1);
        }
        ps_trim(&qhdr->val);

        switch ((qhdr->wkhdr = http_wkhdr_from_ps(qhdr->key))) {
          case HTTP_WKHDR_CONNECTION:
            conn_close |= http_hdr_contains(qhdr->val, "close");
            w->connection_close |= conn_close;
            break;

          case HTTP_WKHDR_TRANSFER_ENCODING:
            /* rfc 2616: §4.4: != "identity" means chunked encoding */
            switch (http_get_token_ps(qhdr->val)) {
              case HTTP_TK_IDENTITY:
                chunked = false;
                break;
              case HTTP_TK_CHUNKED:
                chunked = true;
                break;
              default:
                return PARSE_ERROR;
            }
            break;

          case HTTP_WKHDR_CONTENT_LENGTH:
            clen = memtoip(qhdr->val.b, ps_len(&qhdr->val), &p);
            if (p != qhdr->val.b_end)
                return PARSE_ERROR;
            break;

          default:
            break;
        }
    }

    if (chunked) {
        /* rfc 2616: §4.4: if chunked, then ignore any Content-Length */
        w->chunk_length = 0;
        w->state = HTTP_PARSER_CHUNK_HDR;
    } else {
        w->chunk_length = clen < 0 ? 0 : clen;
        w->state = HTTP_PARSER_BODY;
    }
    req.hdrs     = hdrs.tab;
    req.hdrs_len = hdrs.len;

    q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
    q->qinfo = httpc_qinfo_dup(&req);
    if (q->on_hdrs)
        RETHROW((*q->on_hdrs)(q));
    if (conn_close) {
        w->max_queries = 0;
        if (!w->busy)
            obj_vcall(w, set_busy);
        dlist_for_each_entry_safe_continue(q, q, &w->query_list, query_link) {
            httpc_query_abort(q);
        }
        ob_wipe(&w->ob);
        ob_init(&w->ob);
    }

    return PARSE_OK;
}

static int httpc_parse_body(httpc_t *w, pstream_t *ps)
{
    httpc_query_t *q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
    size_t plen = ps_len(ps);

    if (plen >= w->chunk_length) {
        pstream_t tmp = __ps_get_ps(ps, w->chunk_length);

        RETHROW((*q->on_data)(q, tmp));
        return httpc_query_ok(q);
    }

    if (plen >= w->cfg->on_data_threshold) {
        RETHROW((*q->on_data)(q, *ps));
        w->chunk_length -= plen;
        ps->b = ps->b_end;
    }
    return PARSE_MISSING_DATA;
}

static int httpc_parse_chunk_hdr(httpc_t *w, pstream_t *ps)
{
    pstream_t line, hex;
    uint64_t  len = 0;
    int res;

    res = http_getline(ps, &line);
    if (res)
        return res;
    http_skipspaces(&line);
    hex = ps_get_span(&line, &ctype_ishexdigit);
    http_skipspaces(&line);
    if (unlikely(ps_len(&line)) != 0 && unlikely(line.s[0] != ';'))
        return PARSE_ERROR;
    if (unlikely(ps_len(&hex) == 0) || unlikely(ps_len(&hex) > 16))
        return PARSE_ERROR;
    for (const char *s = hex.s; s < hex.s_end; s++)
        len = (len << 4) | __str_digit_value[*s + 128];
    w->chunk_length = len;
    w->state = len ? HTTP_PARSER_CHUNK : HTTP_PARSER_CHUNK_TRAILER;
    return PARSE_OK;
}

static int httpc_parse_chunk(httpc_t *w, pstream_t *ps)
{
    httpc_query_t *q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
    size_t plen = ps_len(ps);

    if (plen >= w->chunk_length + 2) {
        pstream_t tmp = __ps_get_ps(ps, w->chunk_length);

        if (ps_skipstr(ps, "\r\n"))
            return PARSE_ERROR;
        RETHROW((*q->on_data)(q, tmp));
        w->state = HTTP_PARSER_CHUNK_HDR;
        return PARSE_OK;
    }
    if (plen >= w->cfg->on_data_threshold) {
        RETHROW((*q->on_data)(q, *ps));
        w->chunk_length -= plen;
        ps->b = ps->b_end;
    }
    return PARSE_MISSING_DATA;
}

static int httpc_parse_chunk_trailer(httpc_t *w, pstream_t *ps)
{
    httpc_query_t *q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
    pstream_t line;

    do {
        int res = http_getline(ps, &line);

        if (res)
            return res;
    } while (ps_len(&line));
    return httpc_query_ok(q);
}

static int (*httpc_parsers[])(httpc_t *w, pstream_t *ps) = {
    [HTTP_PARSER_IDLE]          = httpc_parse_idle,
    [HTTP_PARSER_BODY]          = httpc_parse_body,
    [HTTP_PARSER_CHUNK_HDR]     = httpc_parse_chunk_hdr,
    [HTTP_PARSER_CHUNK]         = httpc_parse_chunk,
    [HTTP_PARSER_CHUNK_TRAILER] = httpc_parse_chunk_trailer,
};

/* }}} */
/* HTTPC {{{ */

httpc_cfg_t *httpc_cfg_init(httpc_cfg_t *cfg)
{
    p_clear(cfg, 1);

    cfg->on_data_threshold = _G.on_data_threshold;
    cfg->pipeline_depth    = _G.pipeline_depth_out;
    cfg->noact_delay       = _G.noact_delay;
    cfg->max_queries       = _G.max_queries_out;
    cfg->httpc_cls         = obj_class(httpc);
    return cfg;
}

void httpc_cfg_wipe(httpc_cfg_t *cfg)
{
}

httpc_pool_t *httpc_pool_init(httpc_pool_t *pool)
{
    p_clear(pool, 1);
    dlist_init(&pool->ready_list);
    dlist_init(&pool->busy_list);
    return pool;
}

void httpc_pool_close_clients(httpc_pool_t *pool)
{
    dlist_t lst = DLIST_INIT(lst);

    dlist_splice(&lst, &pool->busy_list);
    dlist_splice(&lst, &pool->ready_list);
    dlist_for_each_safe(it, &lst) {
        obj_release(dlist_entry(it, httpc_t, pool_link));
    }
}

void httpc_pool_wipe(httpc_pool_t *pool)
{
    dlist_for_each_safe(it, &pool->busy_list) {
        httpc_pool_detach(dlist_entry(it, httpc_t, pool_link));
    }
    dlist_for_each_safe(it, &pool->ready_list) {
        httpc_pool_detach(dlist_entry(it, httpc_t, pool_link));
    }
    lstr_wipe(&pool->host);
    httpc_cfg_delete(&pool->cfg);
}

void httpc_pool_detach(httpc_t *w)
{
    if (w->pool) {
        w->pool->len--;
        if (w->pool->len_global)
            (*w->pool->len_global)--;
        dlist_remove(&w->pool_link);
        w->pool = NULL;
    }
}

void httpc_pool_attach(httpc_t *w, httpc_pool_t *pool)
{
    httpc_pool_detach(w);
    w->pool = pool;
    pool->len++;
    if (pool->len_global)
        (*pool->len_global)++;
    if (w->busy) {
        dlist_add(&pool->busy_list, &w->pool_link);
        if (pool->on_busy)
            (*pool->on_busy)(pool, w);
    } else {
        dlist_add(&pool->ready_list, &w->pool_link);
        if (pool->on_ready)
            (*pool->on_ready)(pool, w);
    }
}

httpc_t *httpc_pool_launch(httpc_pool_t *pool)
{
    return httpc_connect(&pool->su, pool->cfg, pool);
}

httpc_t *httpc_pool_get(httpc_pool_t *pool)
{
    httpc_t *httpc;

    if (dlist_is_empty(&pool->ready_list)) {
        if (pool->len >= pool->max_len
        ||  (pool->len_global && *pool->len_global >= pool->max_len_global))
        {
            return NULL;
        }
        httpc = httpc_connect(&pool->su, pool->cfg, pool);
        return httpc->busy ? NULL : httpc;
    }

    httpc = dlist_first_entry(&pool->ready_list, httpc_t, pool_link);
    dlist_move_tail(&pool->ready_list, &httpc->pool_link);
    return httpc;
}

static httpc_t *httpc_init(httpc_t *w)
{
    dlist_init(&w->query_list);
    sb_init(&w->ibuf);
    ob_init(&w->ob);
    w->state = HTTP_PARSER_IDLE;
    return w;
}

static void httpc_wipe(httpc_t *w)
{
    if (w->ev)
        obj_vcall(w, disconnect);
    sb_wipe(&w->ibuf);
    ob_wipe(&w->ob);
    httpc_cfg_delete(&w->cfg);
}

static void httpc_disconnect(httpc_t *w)
{
    httpc_pool_detach(w);
    el_fd_unregister(&w->ev, true);
    dlist_for_each_safe(it, &w->query_list) {
        httpc_query_abort(dlist_entry(it, httpc_query_t, query_link));
    }
}

static void httpc_set_ready(httpc_t *w, bool first)
{
    httpc_pool_t *pool = w->pool;

    assert (w->busy);
    w->busy = false;
    if (pool) {
        dlist_move(&pool->ready_list, &w->pool_link);
        if (pool->on_ready)
            (*pool->on_ready)(pool, w);
    }
}

static void httpc_set_busy(httpc_t *w)
{
    httpc_pool_t *pool = w->pool;

    assert (!w->busy);
    w->busy = true;
    if (pool) {
        dlist_move(&pool->busy_list, &w->pool_link);
        if (pool->on_busy)
            (*pool->on_busy)(pool, w);
    }
}

OBJ_VTABLE(httpc)
    httpc.init       = httpc_init;
    httpc.disconnect = httpc_disconnect;
    httpc.wipe       = httpc_wipe;
    httpc.set_ready  = httpc_set_ready;
    httpc.set_busy   = httpc_set_busy;
OBJ_VTABLE_END()

void httpc_close_gently(httpc_t *w)
{
    w->connection_close = true;
    if (!w->busy)
        obj_vcall(w, set_busy);
    /* let the event loop maybe destroy us later, not now */
    el_fd_set_mask(w->ev, POLLOUT);
}

static void httpc_set_mask(httpc_t *w)
{
    int mask = 0;

    if (!ob_is_empty(&w->ob))
        mask |= POLLOUT;
    if (!dlist_is_empty(&w->query_list))
        mask |= POLLIN;
    el_fd_set_mask(w->ev, mask);
}

static int httpc_on_event(el_t evh, int fd, short events, el_data_t priv)
{
    httpc_t *w = priv.ptr;
    httpc_query_t *q;
    pstream_t ps;
    int res, st = HTTPC_STATUS_INVALID;

    if (events == EL_EVENTS_NOACT)
        goto close;

    if (events & POLLIN) {
        if (sb_read(&w->ibuf, fd, 0) <= 0)
            goto close;

        ps = ps_initsb(&w->ibuf);
        do {
            res = (*httpc_parsers[w->state])(w, &ps);
        } while (res == PARSE_OK);
        if (res < 0) {
            st = res;
            goto close;
        }
        sb_skip_upto(&w->ibuf, ps.s);
    }

    if (unlikely(w->connection_close)) {
        if (dlist_is_empty(&w->query_list) && ob_is_empty(&w->ob))
            goto close;
    }
    res = ob_write(&w->ob, fd);
    if (res < 0 && !ERR_RW_RETRIABLE(errno))
        goto close;
    httpc_set_mask(w);
    return 0;

  close:
    httpc_pool_detach(w);
    if (!dlist_is_empty(&w->query_list)) {
        q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
        if (q->qinfo)
            httpc_query_on_done(q, st);
    }
    obj_vcall(w, disconnect);
    obj_delete(&w);
    return 0;
}

static int httpc_on_connect(el_t evh, int fd, short events, el_data_t priv)
{
    httpc_t *w   = priv.ptr;
    int      res = socket_connect_status(fd);

    if (res > 0) {
        el_fd_set_hook(evh, httpc_on_event);
        httpc_set_mask(w);
        obj_vcall(w, set_ready, true);
    } else
    if (res < 0) {
        obj_vcall(w, disconnect);
        obj_delete(&w);
    }
    return 0;
}

httpc_t *httpc_connect(const sockunion_t *su, httpc_cfg_t *cfg, httpc_pool_t *pool)
{
    httpc_t *w;
    int fd;

    fd = RETHROW_NP(connectx(-1, su, 1, SOCK_STREAM, IPPROTO_TCP, O_NONBLOCK));
    w  = obj_new_of_class(httpc, cfg->httpc_cls);
    w->cfg         = httpc_cfg_dup(cfg);
    w->ev          = el_unref(el_fd_register(fd, POLLOUT, &httpc_on_connect, w));
    w->max_queries = cfg->max_queries;
    el_fd_watch_activity(w->ev, POLLINOUT, w->cfg->noact_delay);
    w->busy        = true;
    if (pool)
        httpc_pool_attach(w, pool);
    return w;
}

httpc_t *httpc_spawn(int fd, httpc_cfg_t *cfg, httpc_pool_t *pool)
{
    httpc_t *w = obj_new_of_class(httpc, cfg->httpc_cls);

    w->cfg         = httpc_cfg_dup(cfg);
    w->ev          = el_unref(el_fd_register(fd, POLLIN, &httpc_on_event, w));
    w->max_queries = cfg->max_queries;
    el_fd_watch_activity(w->ev, POLLINOUT, w->cfg->noact_delay);
    httpc_set_mask(w);
    if (pool)
        httpc_pool_attach(w, pool);
    return w;
}

/* }}} */
/* HTTPC Queries {{{ */

void httpc_query_init(httpc_query_t *q)
{
    p_clear(q, 1);
    dlist_init(&q->query_link);
    sb_init(&q->payload);
}

#define clear_fields_range_(type_t, v, f1, f2) \
    ({ type_t *__v = (v);                      \
       size_t off1 = offsetof(type_t, f1);     \
       size_t off2 = offsetof(type_t, f2);     \
       memset((uint8_t *)__v + off1, 0, off2 - off1); })
#define clear_fields_range(v, f1, f2) \
    clear_fields_range_(typeof(*(v)), v, f1, f2)

void httpc_query_reset(httpc_query_t *q)
{
    dlist_remove(&q->query_link);
    httpc_qinfo_delete(&q->qinfo);
    sb_reset(&q->payload);

    clear_fields_range(q, chunk_hdr_offs, on_hdrs);
}

void httpc_query_wipe(httpc_query_t *q)
{
    dlist_remove(&q->query_link);
    httpc_qinfo_delete(&q->qinfo);
    sb_wipe(&q->payload);
}

void httpc_query_attach(httpc_query_t *q, httpc_t *w)
{
    assert (w->ev && w->max_queries > 0);
    assert (!q->hdrs_started && !q->hdrs_done);
    q->owner = w;
    dlist_add_tail(&w->query_list, &q->query_link);
    if (--w->max_queries == 0) {
        w->connection_close = true;
        if (!w->busy)
            obj_vcall(w, set_busy);
    }
    if (++w->queries >= w->cfg->pipeline_depth && !w->busy)
        obj_vcall(w, set_busy);
}

static int httpc_query_on_data_bufferize(httpc_query_t *q, pstream_t ps)
{
    size_t plen = ps_len(&ps);

    if (unlikely(plen + q->payload.len > q->payload_max_size))
        return HTTPC_STATUS_TOOLARGE;
    sb_add(&q->payload, ps.s, plen);
    return 0;
}

void httpc_bufferize(httpc_query_t *q, unsigned maxsize)
{
    q->payload_max_size = maxsize;
    q->on_data          = &httpc_query_on_data_bufferize;
}

void httpc_query_start(httpc_query_t *q, http_method_t m,
                       lstr_t host, lstr_t uri)
{
    httpc_t  *w  = q->owner;
    outbuf_t *ob = &w->ob;

    assert (!q->hdrs_started && !q->hdrs_done);

    if (w->cfg->use_proxy) {
        ob_addf(ob, "%*pM http://%*pM", LSTR_FMT_ARG(http_method_str[m]),
                LSTR_FMT_ARG(host));
        ob_add_urlencode(ob, uri.s, uri.len);
        ob_adds(ob, " HTTP/1.1\r\n");
    } else {
        ob_add(ob, http_method_str[m].s, http_method_str[m].len);
        ob_adds(ob, " ");
        ob_add_urlencode(ob, uri.s, uri.len);
        ob_addf(ob, " HTTP/1.1\r\n"
                "Host: %*pM\r\n", LSTR_FMT_ARG(host));
    }
    http_update_date_cache(&date_cache_g, lp_getsec());
    ob_add(ob, date_cache_g.buf, sizeof(date_cache_g.buf) - 1);
    if (w->connection_close)
        ob_adds(ob, "Connection: close\r\n");
    q->hdrs_started = true;
}

void httpc_query_hdrs_done(httpc_query_t *q, int clen, bool chunked)
{
    outbuf_t *ob = &q->owner->ob;

    assert (!q->hdrs_done);
    q->hdrs_done = true;

    if (clen >= 0) {
        ob_addf(ob, "Content-Length: %d\r\n\r\n", clen);
        return;
    }
    if (chunked) {
        q->chunked = true;
        ob_adds(ob, "Transfer-Encoding: chunked\r\n");
        /* XXX: no \r\n because http_chunk_patch adds it */
    } else {
        q->clength_hack = true;
        ob_adds(ob, "Content-Length: ");
        q->chunk_hdr_offs    = ob_reserve(ob, CLENGTH_RESERVE);
        ob_adds(ob, "\r\n");
        q->chunk_prev_length = ob->length;
    }
}

void httpc_query_chunk_done_(httpc_query_t *q, outbuf_t *ob)
{
    assert (q->chunk_started);
    q->chunk_started = false;
    http_chunk_patch(ob, ob->sb.data + q->chunk_hdr_offs,
                     ob->length - q->chunk_prev_length);
}

void httpc_query_done(httpc_query_t *q)
{
    outbuf_t *ob = &q->owner->ob;

    assert (q->hdrs_done && !q->query_done && !q->chunk_started);
    if (q->chunked)
        ob_adds(ob, "\r\n0\r\n\r\n");
    if (q->clength_hack) {
        http_clength_patch(ob, ob->sb.data + q->chunk_hdr_offs,
                           ob->length - q->chunk_prev_length);
        q->clength_hack = false;
    }
    q->query_done = true;
    httpc_set_mask(q->owner);
}

void httpc_query_hdrs_add_auth(httpc_query_t *q, lstr_t login, lstr_t passwd)
{
    outbuf_t *ob = &q->owner->ob;
    sb_t *sb;
    sb_b64_ctx_t ctx;
    int oldlen;

    assert (q->hdrs_started && !q->hdrs_done);

    sb = outbuf_sb_start(ob, &oldlen);

    sb_adds(sb, "Authorization: Basic ");
    sb_add_b64_start(sb, 0, -1, &ctx);
    sb_add_b64_update(sb, login.s, login.len, &ctx);
    sb_add_b64_update(sb, ":", 1, &ctx);
    sb_add_b64_update(sb, passwd.s, passwd.len, &ctx);
    sb_add_b64_finish(sb, &ctx);
    sb_adds(sb, "\r\n");

    outbuf_sb_end(ob, oldlen);
}

/* }}} */
