/***************************************************************************/
/*                                                                         */
/* Copyright 2022 INTERSEC SA                                              */
/*                                                                         */
/* Licensed under the Apache License, Version 2.0 (the "License");         */
/* you may not use this file except in compliance with the License.        */
/* You may obtain a copy of the License at                                 */
/*                                                                         */
/*     http://www.apache.org/licenses/LICENSE-2.0                          */
/*                                                                         */
/* Unless required by applicable law or agreed to in writing, software     */
/* distributed under the License is distributed on an "AS IS" BASIS,       */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*/
/* See the License for the specific language governing permissions and     */
/* limitations under the License.                                          */
/*                                                                         */
/***************************************************************************/

#include <lib-common/unix.h>
#include <lib-common/datetime.h>

#include <lib-common/http.h>
#include <lib-common/net/hpack.h>

#include <lib-common/iop.h>
#include <lib-common/core/core.iop.h>

#include <openssl/ssl.h>

#include "httptokens.h"

static struct {
    logger_t logger;
    unsigned http2_conn_count;
} http_g = {
#define _G  http_g
    .logger = LOGGER_INIT_INHERITS(NULL, "http"),
};

/*
 * rfc 2616 TODO list:
 *
 * ETags
 * Range requests
 *
 * Automatically transform chunked-encoding to C-L for HTTP/1.0
 *
 */

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

httpd_trigger_t *httpd_trigger_dup(httpd_trigger_t *cb)
{
    cb->refcnt += 2;
    return cb;
}

void httpd_trigger_delete(httpd_trigger_t **cbp)
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

/* zlib helpers {{{ */

#define HTTP_ZLIB_BUFSIZ   (64 << 10)

static void http_zlib_stream_reset(z_stream *s)
{
    s->next_in  = s->next_out  = NULL;
    s->avail_in = s->avail_out = 0;
}

#define http_zlib_inflate_init(w) \
    ({  typeof(*(w)) *_w = (w);                                   \
                                                                  \
        if (_w->zs.state == NULL) {                               \
            if (inflateInit2(&_w->zs, MAX_WBITS + 32) != Z_OK)    \
                logger_panic(&_G.logger, "zlib error");           \
        }                                                         \
        http_zlib_stream_reset(&_w->zs);                          \
        _w->compressed = true;                                    \
    })

#define http_zlib_reset(w) \
    ({  typeof(*(w)) *_w = (w);                                   \
                                                                  \
        if (_w->compressed) {                                     \
            http_zlib_stream_reset(&_w->zs);                      \
            inflateReset(&_w->zs);                                \
            _w->compressed = false;                               \
        }                                                         \
    })

#define http_zlib_wipe(w) \
    ({  typeof(*(w)) *_w = (w);                            \
                                                           \
        if (_w->zs.state)                                  \
            inflateEnd(&_w->zs);                           \
        _w->compressed = false;                            \
    })

static int http_zlib_inflate(z_stream *s, int *clen,
                             sb_t *out, pstream_t *in, int flush)
{
    int rc;

    s->next_in   = (Bytef *)in->s;
    s->avail_in  = ps_len(in);

    for (;;) {
        size_t sz = MAX(HTTP_ZLIB_BUFSIZ, s->avail_in * 4);

        s->next_out  = (Bytef *)sb_grow(out, sz);
        s->avail_out = sb_avail(out);

        rc = inflate(s, flush ? Z_FINISH : Z_SYNC_FLUSH);
        switch (rc) {
          case Z_BUF_ERROR:
          case Z_OK:
          case Z_STREAM_END:
            __sb_fixlen(out, (char *)s->next_out - out->data);
            if (*clen >= 0) {
                *clen -= (char *)s->next_in - in->s;
            }
            __ps_skip_upto(in, s->next_in);
            break;
          default:
            return rc;
        }

        if (rc == Z_STREAM_END && ps_len(in)) {
            return Z_STREAM_ERROR;
        }
        if (rc == Z_BUF_ERROR) {
            if (s->avail_in) {
                continue;
            }
            if (flush) {
                return Z_STREAM_ERROR;
            }
            return 0;
        }
        return 0;
    }
}

/* }}} */
/* RFC 2616 helpers {{{ */

#define PARSE_RETHROW(e)  ({                                                 \
            int __e = (e);                                                   \
            if (unlikely(__e)) {                                             \
                return __e;                                                  \
            }                                                                \
        })

static inline void http_skipspaces(pstream_t *ps)
{
    while (ps->s < ps->s_end && (ps->s[0] == ' ' || ps->s[0] == '\t'))
        ps->s++;
}

/* rfc 2616, §2.2: Basic rules */
static inline int http_getline(pstream_t *ps, unsigned max_len,
                               pstream_t *out)
{
    const char *p = memmem(ps->s, ps_len(ps), "\r\n", 2);

    if (unlikely(!p)) {
        *out = ps_initptr(NULL, NULL);
        if (ps_len(ps) > max_len) {
            return PARSE_ERROR;
        }
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

    if (ps_len(&ps) != vlen) {
        return false;
    }
    for (size_t i = 0; i < vlen; i++) {
        if (tolower(ps.b[i]) != v[i]) {
            return false;
        }
    }
    return true;
}

static bool http_hdr_contains(pstream_t ps, const char *v)
{
    pstream_t tmp = ps_initptr(NULL, NULL);

    while (ps_get_ps_chr(&ps, ',', &tmp) == 0) {
        ps_trim(&tmp);
        __ps_skip(&ps, 1);

        if (http_hdr_equals(tmp, v)) {
            return true;
        }
    }
    ps_trim(&ps);
    return http_hdr_equals(ps, v);
}

/* rfc 2616: §5.1: Request Line */

static int t_urldecode(httpd_qinfo_t *rq, pstream_t ps)
{
    char *buf  = t_new_raw(char, ps_len(&ps) + 1);
    char *p    = buf;

    rq->vars = ps_initptr(NULL, NULL);

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
            rq->vars = ps;
            break;
        }
        *p++ = c;
    }
    *p++ = '\0';

    path_simplify2(buf, true);
    rq->prefix = ps_initptr(NULL, NULL);
    rq->query  = ps_initstr(buf);
    return 0;
}

static int ps_get_ver(pstream_t *ps)
{
    int i = ps_geti(ps);

    PS_WANT(i >= 0 && i < 128);
    return i;
}

static int t_http_parse_request_line(pstream_t *ps, unsigned max_len,
                                     httpd_qinfo_t *req)
{
    pstream_t line, method, uri;

    do {
        PARSE_RETHROW(http_getline(ps, max_len, &line));
    } while (ps_len(&line) == 0);

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
        return PARSE_ERROR;
#undef  CASE
    }

    uri = ps_initptr(NULL, NULL);
    PS_CHECK(ps_get_ps_chr(&line, ' ', &uri));
    __ps_skip(&line, 1);

    if (ps_skipstr(&uri, "http://") == 0 || ps_skipstr(&uri, "https://") == 0)
    {
        PS_CHECK(ps_get_ps_chr(&uri, '/', &req->host));
    } else {
        p_clear(&req->host, 1);
        if (uri.s[0] != '/' && !ps_memequal(&uri, "*", 1)) {
            return PARSE_ERROR;
        }
    }
    RETHROW(t_urldecode(req, uri));
    PS_CHECK(ps_skipstr(&line, "HTTP/"));
    if (ps_len(&line) == 0 || !isdigit(line.b[0])) {
        return PARSE_ERROR;
    }
    req->http_version  = RETHROW(ps_get_ver(&line)) << 8;
    if (ps_getc(&line) != '.' || ps_len(&line) == 0 || !isdigit(line.b[0])) {
        return PARSE_ERROR;
    }
    req->http_version |= RETHROW(ps_get_ver(&line));
    return ps_len(&line) ? PARSE_ERROR : 0;
}

/* rfc 2616: §6.1: Status Line */

static inline int
http_parse_status_line(pstream_t *ps, unsigned max_len, httpc_qinfo_t *qi)
{
    pstream_t line, code;

    PARSE_RETHROW(http_getline(ps, max_len, &line));

    if (ps_skipstr(&line, "HTTP/")) {
        return PARSE_ERROR;
    }
    if (ps_len(&line) == 0 || !isdigit(line.b[0])) {
        return PARSE_ERROR;
    }
    qi->http_version  = RETHROW(ps_get_ver(&line)) << 8;
    if (ps_getc(&line) != '.' || ps_len(&line) == 0 || !isdigit(line.b[0])) {
        return PARSE_ERROR;
    }
    qi->http_version |= RETHROW(ps_get_ver(&line));
    __ps_skip(&line, 1);

    if (ps_get_ps_chr(&line, ' ', &code) || ps_len(&code) != 3) {
        return PARSE_ERROR;
    }
    __ps_skip(&line, 1);

    qi->code = ps_geti(&code);
    if ((int)qi->code < 100 || (int)qi->code >= 600) {
        return PARSE_ERROR;
    }
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

    if (w->queries == 0) {
        q->ob = &w->ob;
    }
    /* ensure refcount is 3: owned, unanwsered, unparsed */
    obj_retain(q);
    obj_retain(q);
    q->owner = w;
    dlist_add_tail(&w->query_list, &q->query_link);
    if (cb) {
        q->trig_cb = httpd_trigger_dup(cb);
    }
    return q;
}

static ALWAYS_INLINE void httpd_query_detach(httpd_query_t *q)
{
    httpd_t *w = q->owner;

    if (w) {
        if (!q->own_ob) {
            q->ob = NULL;
        }
        dlist_remove(&q->query_link);
        if (q->parsed) {
            w->queries--;
        }
        w->queries_done -= q->answered;
        q->owner = NULL;
        obj_release(&q);
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
        if (q->trig_cb->on_query_wipe) {
            q->trig_cb->on_query_wipe(q);
        }
        httpd_trigger_delete(&q->trig_cb);
    }
    if (q->own_ob) {
        ob_delete(&q->ob);
    }
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
    if (!inf) {
        return;
    }
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
    ob_adds(ob, "Accept-Encoding: identity, gzip, deflate\r\n");

    /* XXX: For CORS purposes, allow all origins for now */
    ob_adds(ob, "Access-Control-Allow-Origin: *\r\n");

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
            if (q->owner) {
                q->owner->connection_close = true;
            }
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


__attribute__((format(printf, 4, 0)))
static void httpd_notify_status(httpd_t *w, httpd_query_t *q, int handler,
                              const char *fmt, va_list va);

void httpd_reply_done(httpd_query_t *q)
{
    va_list va;
    outbuf_t *ob = httpd_get_ob(q);

    assert (q->hdrs_done && !q->answered && !q->chunk_started);
    if (q->chunked) {
        ob_adds(ob, "\r\n0\r\n\r\n");
    }
    if (q->clength_hack) {
        http_clength_patch(ob, ob->sb.data + q->chunk_hdr_offs,
                           ob->length - q->chunk_prev_length);
        q->clength_hack = false;
    }
    httpd_notify_status(q->owner, q, HTTPD_QUERY_STATUS_ANSWERED, "", va);
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
    if (q->answered || q->hdrs_started) {
        return;
    }
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
    if (q->answered || q->hdrs_started) {
        return;
    }

    httpd_reply_hdrs_start(q, HTTP_CODE_ACCEPTED, false);
    httpd_reply_hdrs_done(q, 0, false);
    httpd_reply_done(q);
}

void httpd_reject_(httpd_query_t *q, int code, const char *fmt, ...)
{
    va_list ap;
    outbuf_t *ob;

    if (q->answered || q->hdrs_started) {
        return;
    }

    ob = httpd_reply_hdrs_start(q, code, false);
    ob_adds(ob, "Content-Type: text/html\r\n");
    httpd_reply_hdrs_done(q, -1, false);

    ob_addf(ob, "<html><body><h1>%d - %*pM</h1><p>",
            code, LSTR_FMT_ARG(http_code_to_str(code)));
    va_start(ap, fmt);
    ob_addvf(ob, fmt, ap);
    va_end(ap);
    ob_adds(ob, "</p></body></html>\r\n");

    va_start(ap, fmt);
    httpd_notify_status(q->owner, q, HTTPD_QUERY_STATUS_ANSWERED, fmt, ap);
    va_end(ap);
    httpd_reply_done(q);
}

void httpd_reject_unauthorized(httpd_query_t *q, lstr_t auth_realm)
{
    const lstr_t body = LSTR("<html><body>"
                             "<h1>401 - Authentication required</h1>"
                             "</body></html>\r\n");
    va_list va;
    outbuf_t *ob;

    if (q->answered || q->hdrs_started) {
        return;
    }

    ob = httpd_reply_hdrs_start(q, HTTP_CODE_UNAUTHORIZED, false);
    ob_adds(ob, "Content-Type: text/html\r\n");
    ob_addf(ob, "WWW-Authenticate: Basic realm=\"%*pM\"\r\n",
            LSTR_FMT_ARG(auth_realm));
    httpd_reply_hdrs_done(q, body.len, false);
    ob_add(ob, body.s, body.len);

    httpd_notify_status(q->owner, q, HTTP_CODE_UNAUTHORIZED, "", va);
    httpd_reply_done(q);
}

/* }}} */
/* HTTPD Triggers {{{ */

static httpd_trigger_node_t *
httpd_trigger_node_new(httpd_trigger_node_t *parent, lstr_t path)
{
    httpd_trigger_node_t *node;
    uint32_t pos;

    pos = qm_put(http_path, &parent->childs, &path, NULL, 0);
    if (pos & QHASH_COLLISION) {
        return parent->childs.values[pos & ~QHASH_COLLISION];
    }

    parent->childs.values[pos] = node =
        p_new_extra(httpd_trigger_node_t, path.len + 1);
    qm_init_cached(http_path, &node->childs);
    memcpy(node->path, path.s, path.len + 1);

    /* Ensure the key point to a valid string since path may be deallocated */
    parent->childs.keys[pos] = LSTR_INIT_V(node->path, path.len);
    return node;
}

static void httpd_trigger_node_wipe(httpd_trigger_node_t *node);
GENERIC_DELETE(httpd_trigger_node_t, httpd_trigger_node);

static void httpd_trigger_node_wipe(httpd_trigger_node_t *node)
{
    httpd_trigger_delete(&node->cb);
    qm_deep_wipe(http_path, &node->childs, IGNORE, httpd_trigger_node_delete);
}

bool httpd_trigger_register_flags(httpd_trigger_node_t *n, const char *path,
                                  httpd_trigger_t *cb, bool overwrite)
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
    if (!overwrite && n->cb) {
        return false;
    }
    httpd_trigger_delete(&n->cb);
    n->cb = httpd_trigger_dup(cb);
    if (unlikely(cb->query_cls == NULL)) {
        cb->query_cls = obj_class(httpd_query);
    }
    return true;
}

static bool httpd_trigger_unregister__(httpd_trigger_node_t *n, const char *path,
                                       httpd_trigger_t *what, bool *res)
{
    while (*path == '/')
        path++;

    if (!*path) {
        if (!what || n->cb == what) {
            httpd_trigger_delete(&n->cb);
            *res = true;
        } else {
            *res = false;
        }
    } else {
        const char *q = strchrnul(path, '/');
        lstr_t      s = LSTR_INIT(path, q - path);
        int pos       = qm_find(http_path, &n->childs, &s);

        if (pos < 0) {
            return false;
        }
        if (httpd_trigger_unregister__(n->childs.values[pos], q, what, res)) {
            httpd_trigger_node_delete(&n->childs.values[pos]);
            qm_del_at(http_path, &n->childs, pos);
        }
    }
    return qm_len(http_path, &n->childs) == 0;
}

bool httpd_trigger_unregister_(httpd_trigger_node_t *n, const char *path,
                               httpd_trigger_t *what)
{
    bool res = false;

    httpd_trigger_unregister__(n, path, what, &res);
    return res;
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
        if (pos < 0) {
            break;
        }
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

static inline void t_ps_get_http_var_parse_elem(pstream_t elem, lstr_t *out)
{
    if (memchr(elem.p, '%', ps_len(&elem))) {
        sb_t sb;

        t_sb_init(&sb, ps_len(&elem));
        sb_add_lstr_urldecode(&sb, LSTR_PS_V(&elem));
        *out = lstr_init_(sb.data, sb.len, MEM_STACK);
    } else {
        *out = LSTR_PS_V(&elem);
    }
}

int t_ps_get_http_var(pstream_t *ps, lstr_t *key, lstr_t *value)
{
    pstream_t key_ps, value_ps;

    RETHROW(ps_get_ps_chr_and_skip(ps, '=', &key_ps));
    THROW_ERR_IF(ps_done(&key_ps));

    if (ps_get_ps_chr_and_skip(ps, '&', &value_ps) < 0) {
        RETHROW(ps_get_ps(ps, ps_len(ps), &value_ps));
    }

    t_ps_get_http_var_parse_elem(key_ps,   key);
    t_ps_get_http_var_parse_elem(value_ps, value);

    return 0;
}

__attribute__((format(printf, 4, 0)))
static void httpd_notify_status(httpd_t *w, httpd_query_t *q, int handler,
                              const char *fmt, va_list va)
{
    if (!q->status_sent) {
        q->status_sent = true;

        if (w && w->on_status) {
            (*w->on_status)(w, q, handler, fmt, va);
        }
    }
}

static void httpd_set_mask(httpd_t *w)
{
    int mask;

    if (w->queries >= w->cfg->pipeline_depth
    ||  w->ob.length >= (int)w->cfg->outbuf_max_size
    ||  w->state == HTTP_PARSER_CLOSE)
    {
        mask = 0;
    } else {
        mask = POLLIN;
    }

    if (!ob_is_empty(&w->ob)) {
        mask |= POLLOUT;
    }

    if (w->ssl) {
        if (SSL_want_read(w->ssl)) {
            mask |= POLLIN;
        }
        if (SSL_want_write(w->ssl)) {
            mask |= POLLOUT;
        }
    }

    el_fd_set_mask(w->ev, mask);
}

static void httpd_flush_answered(httpd_t *w)
{
    dlist_for_each_entry(httpd_query_t, q, &w->query_list, query_link) {
        if (q->own_ob) {
            ob_merge_delete(&w->ob, &q->ob);
            q->own_ob = false;
        }
        if (!q->answered) {
            q->ob = &w->ob;
            break;
        }
        if (likely(q->parsed)) {
            httpd_query_detach(q);
        }
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
    w->queries++;
    httpd_flush_answered(w);
    if (w->connection_close) {
        w->state = HTTP_PARSER_CLOSE;
    } else {
        w->state = HTTP_PARSER_IDLE;
    }
    w->chunk_length = 0;
    obj_release(&q);
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
        if (dlist_is_first(&w->query_list, &q->query_link)) {
            httpd_flush_answered(w);
        }
    }
    q->expect100cont = false;
    obj_release(&q);
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
        if (ps_len(ps) > w->cfg->header_size_max) {
            q = httpd_query_create(w, NULL);
            httpd_reject(q, FORBIDDEN, "Headers exceed %d octets",
                         w->cfg->header_size_max);
            goto unrecoverable_error;
        }
        w->chunk_length = ps_len(ps);
        return PARSE_MISSING_DATA;
    }

    if (--w->max_queries == 0) {
        w->connection_close = true;
    }

    http_zlib_reset(w);
    req.hdrs_ps = ps_initptr(ps->s, p + 4);

    switch (t_http_parse_request_line(ps, w->cfg->header_line_max, &req)) {
      case PARSE_ERROR:
        q = httpd_query_create(w, NULL);
        httpd_reject(q, BAD_REQUEST, "Invalid request line");
        goto unrecoverable_error;

      case PARSE_MISSING_DATA:
        return PARSE_MISSING_DATA;

      default:
        break;
    }

    if ((unsigned)req.method < countof(w->cfg->roots)) {
        cb = httpd_trigger_resolve(&w->cfg->roots[req.method], &req);
    }
    q = httpd_query_create(w, cb);
    q->received_hdr_length = ps_len(&req.hdrs_ps);
    q->http_version = req.http_version;
    q->qinfo        = &req;
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
    t_qv_init(&hdrs, 64);

    while (!ps_done(&buf)) {
        http_qhdr_t *qhdr = qv_growlen(&hdrs, 1);

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
            if (ps_done(&buf)) {
                break;
            }
            if (buf.s[0] != '\t' && buf.s[0] != ' ') {
                break;
            }
            __ps_skip(&buf, 1);
        }
        ps_trim(&qhdr->val);

        switch ((qhdr->wkhdr = http_wkhdr_from_ps(qhdr->key))) {
          case HTTP_WKHDR_HOST:
            if (ps_len(&req.host) == 0) {
                req.host = qhdr->val;
            }
            qv_shrink(&hdrs, 1);
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

          case HTTP_WKHDR_CONTENT_ENCODING:
            switch (http_get_token_ps(qhdr->val)) {
              case HTTP_TK_DEFLATE:
              case HTTP_TK_GZIP:
              case HTTP_TK_X_GZIP:
                http_zlib_inflate_init(w);
                qv_shrink(&hdrs, 1);
                break;
              default:
                http_zlib_reset(w);
                break;
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
    if (q->qinfo == &req) {
        q->qinfo = NULL;
    }
    httpd_query_reply_100continue_(q);
    return PARSE_OK;

  unrecoverable_error:
    if (q->qinfo == &req) {
        q->qinfo = NULL;
    }
    w->connection_close = true;
    httpd_query_done(w, q);
    return PARSE_ERROR;
}

static inline int
httpd_flush_data(httpd_t *w, httpd_query_t *q, pstream_t *ps, bool done)
{
    q->received_body_length += ps_len(ps);

    if (q->on_data) {
        if (w->compressed && !ps_done(ps)) {
            t_scope;
            sb_t zbuf;

            t_sb_init(&zbuf, HTTP_ZLIB_BUFSIZ);
            if (http_zlib_inflate(&w->zs, &w->chunk_length, &zbuf, ps, done)) {
                goto zlib_error;
            }
            q->on_data(q, ps_initsb(&zbuf));
            return PARSE_OK;
        }
        q->on_data(q, *ps);
    }
    w->chunk_length -= ps_len(ps);
    ps->b = ps->b_end;
    return PARSE_OK;

  zlib_error:
    httpd_reject(q, BAD_REQUEST, "Invalid compressed data");
    w->connection_close = true;
    httpd_query_done(w, q);
    return PARSE_ERROR;
}

static int httpd_parse_body(httpd_t *w, pstream_t *ps)
{
    httpd_query_t *q = dlist_last_entry(&w->query_list, httpd_query_t, query_link);
    ssize_t plen = ps_len(ps);

    q->expect100cont = false;
    assert (w->chunk_length >= 0);
    if (plen >= w->chunk_length) {
        pstream_t tmp = __ps_get_ps(ps, w->chunk_length);

        RETHROW(httpd_flush_data(w, q, &tmp, true));
        if (q->on_done) {
            q->on_done(q);
        }
        httpd_query_done(w, q);
        return PARSE_OK;
    }

    if (plen >= w->cfg->on_data_threshold) {
        RETHROW(httpd_flush_data(w, q, ps, false));
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
    const char *orig = ps->s;
    pstream_t line, hex;
    uint64_t  len = 0;
    int res;

    q->expect100cont = false;
    res = http_getline(ps, w->cfg->header_line_max, &line);
    if (res > 0) {
        return res;
    }
    if (res < 0) {
        goto cancel_query;
    }
    http_skipspaces(&line);
    hex = ps_get_span(&line, &ctype_ishexdigit);
    http_skipspaces(&line);
    if (unlikely(ps_len(&line)) != 0 && unlikely(line.s[0] != ';')) {
        goto cancel_query;
    }
    if (unlikely(ps_len(&hex) == 0) || unlikely(ps_len(&hex) > 16)) {
        goto cancel_query;
    }
    for (const char *s = hex.s; s < hex.s_end; s++)
        len = (len << 4) | __str_digit_value[*s + 128];
    w->chunk_length = len;
    w->state = len ? HTTP_PARSER_CHUNK : HTTP_PARSER_CHUNK_TRAILER;
    q->received_body_length += ps->s - orig;
    return PARSE_OK;

  cancel_query:
    httpd_reject(q, BAD_REQUEST, "Chunked header is unparseable");
    w->connection_close = true;
    httpd_query_done(w, q);
    return PARSE_ERROR;
}

static int httpd_parse_chunk(httpd_t *w, pstream_t *ps)
{
    httpd_query_t *q = dlist_last_entry(&w->query_list, httpd_query_t, query_link);
    ssize_t plen = ps_len(ps);

    assert (w->chunk_length >= 0);
    if (plen >= w->chunk_length + 2) {
        pstream_t tmp = __ps_get_ps(ps, w->chunk_length);

        if (ps_skipstr(ps, "\r\n")) {
            httpd_reject(q, BAD_REQUEST, "Chunked header is unparseable");
            w->connection_close = true;
            httpd_query_done(w, q);
            return PARSE_ERROR;
        }
        RETHROW(httpd_flush_data(w, q, &tmp, false));
        w->state = HTTP_PARSER_CHUNK_HDR;
        return PARSE_OK;
    }
    if (plen >= w->cfg->on_data_threshold) {
        RETHROW(httpd_flush_data(w, q, ps, false));
    }
    return PARSE_MISSING_DATA;
}

static int httpd_parse_chunk_trailer(httpd_t *w, pstream_t *ps)
{
    httpd_query_t *q = dlist_last_entry(&w->query_list, httpd_query_t, query_link);
    const char *orig = ps->s;
    pstream_t line;

    do {
        int res = (http_getline(ps, w->cfg->header_line_max, &line));

        if (res < 0) {
            httpd_reject(q, BAD_REQUEST, "Trailer headers are unparseable");
            w->connection_close = true;
            httpd_query_done(w, q);
            return PARSE_ERROR;
        }
        if (res > 0) {
            return res;
        }
    } while (ps_len(&line));

    q->received_body_length += ps->s - orig;
    if (q->on_done) {
        q->on_done(q);
    }
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
    core__httpd_cfg__t iop_cfg;

    p_clear(cfg, 1);

    dlist_init(&cfg->httpd_list);
    cfg->httpd_cls = obj_class(httpd);

    iop_init(core__httpd_cfg, &iop_cfg);
    /* Default configuration must succeed. */
    httpd_cfg_from_iop(cfg, &iop_cfg);

    for (int i = 0; i < countof(cfg->roots); i++) {
        qm_init_cached(http_path, &cfg->roots[i].childs);
    }
    return cfg;
}

int httpd_cfg_from_iop(httpd_cfg_t *cfg, const core__httpd_cfg__t *iop_cfg)
{
    THROW_ERR_UNLESS(expect(!cfg->ssl_ctx));
    cfg->outbuf_max_size    = iop_cfg->outbuf_max_size;
    cfg->pipeline_depth     = iop_cfg->pipeline_depth;
    cfg->noact_delay        = iop_cfg->noact_delay;
    cfg->max_queries        = iop_cfg->max_queries;
    cfg->max_conns          = iop_cfg->max_conns_in;
    cfg->on_data_threshold  = iop_cfg->on_data_threshold;
    cfg->header_line_max    = iop_cfg->header_line_max;
    cfg->header_size_max    = iop_cfg->header_size_max;

    if (iop_cfg->tls) {
        core__tls_cert_and_key__t *data;
        SB_1k(errbuf);

        data = IOP_UNION_GET(core__tls_cfg, iop_cfg->tls, data);
        if (!data) {
            /* If a keyname has been provided in the configuration, it
             * should have been replaced by the actual TLS data. */
            logger_panic(&_G.logger, "TLS data are not provided");
        }

        cfg->ssl_ctx = ssl_ctx_new_tls(TLS_server_method(),
                                       data->key, data->cert, SSL_VERIFY_NONE,
                                       NULL, &errbuf);
        if (!cfg->ssl_ctx) {
            logger_fatal(&_G.logger, "couldn't initialize SSL_CTX: %*pM",
                         SB_FMT_ARG(&errbuf));
        }
    }

    return 0;
}

void httpd_cfg_wipe(httpd_cfg_t *cfg)
{
    for (int i = 0; i < countof(cfg->roots); i++) {
        httpd_trigger_node_wipe(&cfg->roots[i]);
    }
    if (cfg->ssl_ctx) {
        SSL_CTX_free(cfg->ssl_ctx);
        cfg->ssl_ctx = NULL;
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
    if (w->on_status) {
        va_list va;

        dlist_for_each(it, &w->query_list) {
            httpd_notify_status(w, dlist_entry(it, httpd_query_t, query_link),
                              HTTPD_QUERY_STATUS_CANCEL, "Query cancelled", va);
        }
    }
    if (w->on_disconnect) {
        (*w->on_disconnect)(w);
    }
    el_unregister(&w->ev);
    sb_wipe(&w->ibuf);
    ob_wipe(&w->ob);
    http_zlib_wipe(w);
    dlist_for_each(it, &w->query_list) {
        httpd_query_detach(dlist_entry(it, httpd_query_t, query_link));
    }
    w->cfg->nb_conns--;
    dlist_remove(&w->httpd_link);
    httpd_cfg_delete(&w->cfg);
    lstr_wipe(&w->peer_address);
    SSL_free(w->ssl);
    w->ssl = NULL;
}

OBJ_VTABLE(httpd)
    httpd.init = httpd_init;
    httpd.wipe = httpd_wipe;
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

        if (hdr->wkhdr != HTTP_WKHDR_AUTHORIZATION) {
            continue;
        }
        v = hdr->val;
        ps_skipspaces(&v);
        PS_CHECK(ps_skipcasestr(&v, "basic"));
        ps_trim(&v);

        len = ps_len(&v);
        t_sb_init(&sb, len + 1);
        PS_CHECK(sb_add_unb64(&sb, v.s, len));
        colon = strchr(sb.data, ':');
        if (!colon) {
            return -1;
        }
        *user    = ps_initptr(sb.data, colon);
        *colon++ = '\0';
        *pw      = ps_initptr(colon, sb_end(&sb));
        return 0;
    }

    *pw = *user = ps_initptr(NULL, NULL);
    return 0;
}

static int parse_qvalue(pstream_t *ps)
{
    int res;

    /* is there a ';' ? */
    if (ps_skipc(ps, ';') < 0) {
        return 1000;
    }
    ps_skipspaces(ps);

    /* parse q= */
    RETHROW(ps_skipc(ps, 'q'));
    ps_skipspaces(ps);
    RETHROW(ps_skipc(ps, '='));
    ps_skipspaces(ps);

    /* slopily parse 1[.000] || 0[.nnn] */
    switch (ps_getc(ps)) {
      case '0': res = 0; break;
      case '1': res = 1; break;
      default:
        return -1;
    }
    if (ps_skipc(ps, '.') == 0) {
        for (int i = 0; i < 3; i++) {
            if (ps_has(ps, 1) && isdigit(ps->s[0])) {
                res  = 10 * res + __ps_getc(ps) - '0';
            } else {
                res *= 10;
            }
        }
        if (res > 1000) {
            res = 1000;
        }
    } else {
        res *= 1000;
    }
    ps_skipspaces(ps);
    return res;
}

static int parse_accept_enc(pstream_t ps)
{
    int res_valid = 0, res_rej = 0, res_star = 0;
    int q;
    pstream_t v;

    ps_skipspaces(&ps);
    while (!ps_done(&ps)) {
        bool is_star = false;

        if (*ps.s == '*') {
            is_star = true;
            __ps_skip(&ps, 1);
        } else {
            v = ps_get_cspan(&ps, &http_non_token);
        }
        ps_skipspaces(&ps);
        q = RETHROW(parse_qvalue(&ps));
        switch (ps_getc(&ps)) {
          case ',':
            ps_skipspaces(&ps);
            break;
          case -1:
            break;
          default:
            return -1;
        }

        if (is_star) {
            res_star = q ? HTTPD_ACCEPT_ENC_ANY : 0;
        } else {
            switch (http_get_token_ps(v)) {
              case HTTP_TK_X_GZIP:
              case HTTP_TK_GZIP:
                if (q) {
                    res_valid |= HTTPD_ACCEPT_ENC_GZIP;
                } else {
                    res_rej   |= HTTPD_ACCEPT_ENC_GZIP;
                }
                break;
              case HTTP_TK_X_COMPRESS:
              case HTTP_TK_COMPRESS:
                if (q) {
                    res_valid |= HTTPD_ACCEPT_ENC_COMPRESS;
                } else {
                    res_rej   |= HTTPD_ACCEPT_ENC_COMPRESS;
                }
                break;
              case HTTP_TK_DEFLATE:
                if (q) {
                    res_valid |= HTTPD_ACCEPT_ENC_DEFLATE;
                } else {
                    res_rej   |= HTTPD_ACCEPT_ENC_DEFLATE;
                }
                break;
              default: /* Ignore "identity" or non RFC Accept-Encodings */
                break;
            }
        }
    }

    return (res_valid | res_star) & ~res_rej;
}

int httpd_qinfo_accept_enc_get(const httpd_qinfo_t *info)
{
    int res = 0;

    for (int i = info->hdrs_len; i-- > 0; ) {
        const http_qhdr_t *hdr = info->hdrs + i;

        if (hdr->wkhdr != HTTP_WKHDR_ACCEPT_ENCODING) {
            continue;
        }

        if ((res = parse_accept_enc(hdr->val)) >= 0) {
            return res;
        }
        /* ignore malformed header */
    }
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
        int                   method = req->method;
        lstr_t                ms     = http_method_str[method];
        httpd_trigger_node_t *n      = &w->cfg->roots[method];

        if (n->cb || qm_len(http_path, &n->childs)) {
            SB_1k(escaped);

            sb_add_lstr_xmlescape(&escaped, LSTR_PS_V(&req->query));
            httpd_reject(q, NOT_FOUND,
                         "%*pM %*pM HTTP/1.%d", LSTR_FMT_ARG(ms),
                         SB_FMT_ARG(&escaped),
                         HTTP_MINOR(req->http_version));
        } else
        if (method == HTTP_METHOD_OPTIONS) {
            /* For CORS purposes, handle OPTIONS if not handled above */
            outbuf_t *ob = httpd_reply_hdrs_start(q, HTTP_CODE_NO_CONTENT,
                                                  false);

            ob_adds(ob, "Access-Control-Allow-Methods: "
                    "POST, GET, OPTIONS\r\n");
            ob_adds(ob, "Access-Control-Allow-Headers: "
                    "Authorization, Content-Type\r\n");

            httpd_reply_hdrs_done(q, 0, false);
            httpd_reply_done(q);
        } else {
            httpd_reject(q, NOT_IMPLEMENTED,
                         "no handler for %*pM", LSTR_FMT_ARG(ms));
        }
    }
}

static void httpd_do_trace(httpd_t *w, httpd_query_t *q, httpd_qinfo_t *req)
{
    httpd_reject(q, METHOD_NOT_ALLOWED, "TRACE method is not allowed");
}

static int httpd_on_event(el_t evh, int fd, short events, data_t priv)
{
    httpd_t *w = priv.ptr;
    pstream_t ps;

    if (events == EL_EVENTS_NOACT) {
        goto close;
    }

    if (events & POLLIN) {
        int ret;

        ret = w->ssl ?
            ssl_sb_read(&w->ibuf, w->ssl, 0):
            sb_read(&w->ibuf, fd, 0);
        if (ret <= 0) {
            if (ret == 0 || !ERR_RW_RETRIABLE(errno)) {
                goto close;
            }
            goto write;
        }

        ps = ps_initsb(&w->ibuf);
        do {
            ret = (*httpd_parsers[w->state])(w, &ps);
        } while (ret == PARSE_OK);
        sb_skip_upto(&w->ibuf, ps.s);
    }

  write:
    {
        int oldlen = w->ob.length;
        int ret;

        ret = w->ssl ?
            ob_write_with(&w->ob, fd, ssl_writev, w->ssl) :
            ob_write(&w->ob, fd);
        if (ret < 0 && !ERR_RW_RETRIABLE(errno)) {
            goto close;
        }

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
        if (w->queries == 0 && ob_is_empty(&w->ob)) {
            /* XXX We call shutdown(…, SHUT_RW) to force TCP to flush our
             * writing buffer and protect our responses against a TCP RST
             * which could be emitted by close() if there is some pending data
             * in the read buffer (think about pipelining). */
            shutdown(fd, SHUT_WR);
            goto close;
        }
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
        if (!q->parsed) {
            obj_release(&q);
            if (!q->answered) {
                obj_release(&q);
            }
        }
    }
    obj_delete(&w);
    return 0;
}

static int
httpd_tls_handshake(el_t evh, int fd, short events, data_t priv)
{
    httpd_t *w = priv.ptr;

    switch (ssl_do_handshake(w->ssl, evh, fd, NULL)) {
      case SSL_HANDSHAKE_SUCCESS:
        el_fd_set_mask(evh, POLLIN);
        el_fd_set_hook(evh, httpd_on_event);
        break;
      case SSL_HANDSHAKE_PENDING:
        break;
      case SSL_HANDSHAKE_CLOSED:
        obj_delete(&w);
        break;
      case SSL_HANDSHAKE_ERROR:
        obj_delete(&w);
        return -1;
    }

    return 0;
}

static int
httpd_on_accept(el_t evh, int fd, short events, data_t priv)
{
    httpd_cfg_t *cfg = priv.ptr;
    int sock;
    sockunion_t su;

    while ((sock = acceptx_get_addr(fd, O_NONBLOCK, &su)) >= 0) {
        if (cfg->nb_conns >= cfg->max_conns) {
            close(sock);
        } else {
            httpd_spawn(sock, cfg)->peer_su = su;
        }
    }
    return 0;
}

el_t httpd_listen(sockunion_t *su, httpd_cfg_t *cfg)
{
    int fd;

    fd = listenx(-1, su, 1, SOCK_STREAM, IPPROTO_TCP, O_NONBLOCK);
    if (fd < 0) {
        return NULL;
    }
    return el_fd_register(fd, true, POLLIN, httpd_on_accept,
                          httpd_cfg_retain(cfg));
}

void httpd_unlisten(el_t *ev)
{
    if (*ev) {
        httpd_cfg_t *cfg = el_unregister(ev).ptr;

        dlist_for_each(it, &cfg->httpd_list) {
            httpd_close_gently(dlist_entry(it, httpd_t, httpd_link));
        }
        httpd_cfg_delete(&cfg);
    }
}

httpd_t *httpd_spawn(int fd, httpd_cfg_t *cfg)
{
    httpd_t *w = obj_new_of_class(httpd, cfg->httpd_cls);
    el_fd_f *el_cb = cfg->ssl_ctx ? &httpd_tls_handshake : &httpd_on_event;

    cfg->nb_conns++;
    w->cfg         = httpd_cfg_retain(cfg);
    w->ev          = el_fd_register(fd, true, POLLIN, el_cb, w);
    w->max_queries = cfg->max_queries;
    if (cfg->ssl_ctx) {
        w->ssl = SSL_new(cfg->ssl_ctx);
        assert (w->ssl);
        SSL_set_fd(w->ssl, fd);
        SSL_set_accept_state(w->ssl);
    }

    el_fd_watch_activity(w->ev, POLLINOUT, w->cfg->noact_delay);
    dlist_add_tail(&cfg->httpd_list, &w->httpd_link);
    if (w->on_accept) {
        (*w->on_accept)(w);
    }
    return w;
}

lstr_t   httpd_get_peer_address(httpd_t * w)
{
    if (!w->peer_address.len) {
        t_scope;

        w->peer_address = lstr_dup(t_addr_fmt_lstr(&w->peer_su));
    }

    return lstr_dupc(w->peer_address);
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
        if (--w->queries < w->cfg->pipeline_depth && w->max_queries && w->busy) {
            obj_vcall(w, set_ready, false);
        }
        q->owner = NULL;
    }
    dlist_remove(&q->query_link);
    /* XXX: call the httpc_t's notifier first to ensure qinfo is still set */
    if (w && w->on_query_done) {
        (*w->on_query_done)(w, q, status);
    }
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
        logger_trace(&_G.logger, 0, "UHOH spurious data from the HTTP "
                     "server: %*pM", (int)ps_len(ps), ps->s);
        return PARSE_ERROR;
    }

    if ((p = memmem(ps->s + start, ps_len(ps) - start, "\r\n\r\n", 4)) == NULL) {
        if (ps_len(ps) > w->cfg->header_size_max) {
            return PARSE_ERROR;
        }
        w->chunk_length = ps_len(ps);
        return PARSE_MISSING_DATA;
    }

    http_zlib_reset(w);
    req.hdrs_ps = ps_initptr(ps->s, p + 4);
    res = http_parse_status_line(ps, w->cfg->header_line_max, &req);
    if (res) {
        return res;
    }

    buf = __ps_get_ps_upto(ps, p + 2);
    __ps_skip_upto(ps, p + 4);
    t_qv_init(&hdrs, 64);

    while (!ps_done(&buf)) {
        http_qhdr_t *qhdr = qv_growlen(&hdrs, 1);

        /* TODO: normalize, make "lists" */
        qhdr->key = ps_get_cspan(&buf, &http_non_token);
        if (ps_len(&qhdr->key) == 0 || __ps_getc(&buf) != ':') {
            return PARSE_ERROR;
        }
        qhdr->val.s = buf.s;
        for (;;) {
            ps_skip_afterchr(&buf, '\r');
            if (__ps_getc(&buf) != '\n') {
                return PARSE_ERROR;
            }
            qhdr->val.s_end = buf.s - 2;
            if (ps_done(&buf)) {
                break;
            }
            if (buf.s[0] != '\t' && buf.s[0] != ' ') {
                break;
            }
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
            if (p != qhdr->val.b_end) {
                return PARSE_ERROR;
            }
            break;

          case HTTP_WKHDR_CONTENT_ENCODING:
            switch (http_get_token_ps(qhdr->val)) {
              case HTTP_TK_DEFLATE:
              case HTTP_TK_GZIP:
              case HTTP_TK_X_GZIP:
                http_zlib_inflate_init(w);
                qv_shrink(&hdrs, 1);
                break;
              default:
                http_zlib_reset(w);
                break;
            }
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
        /* rfc 2616: §4.4: support no Content-Length */
        if (clen < 0 && req.code == HTTP_CODE_NO_CONTENT) {
            /* due to code 204 (No Content) */
            w->chunk_length = 0;
        } else {
            /* or followed by close */
            w->chunk_length = clen;
        }
        w->state = HTTP_PARSER_BODY;
    }
    req.hdrs     = hdrs.tab;
    req.hdrs_len = hdrs.len;

    q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);

    if (req.code >= 100 && req.code < 200) {
        w->state = HTTP_PARSER_IDLE;

        /* rfc 2616: §10.1: A client MUST be prepared to accept one or more
         * 1xx status responses prior to a regular response.
         *
         * Since HTTP/1.0 did not define any 1xx status codes, servers MUST
         * NOT send a 1xx response to an HTTP/1.0 client except under
         * experimental conditions
         */
        if (req.http_version == HTTP_1_0) {
            return PARSE_ERROR;
        } else
        if (req.code != HTTP_CODE_CONTINUE) {
            return PARSE_OK;
        }

        if (q->expect100cont) {
            /* Temporary set the qinfo to the 100 Continue header.
             */
            q->qinfo = &req;
            (*q->on_100cont)(q);
            q->qinfo = NULL;
        }
        q->expect100cont = false;

        return PARSE_OK;
    }

    if (q->expect100cont && (req.code >= 200 && req.code < 300)) {
        return HTTPC_STATUS_EXP100CONT;
    }

    q->received_hdr_length = ps_len(&req.hdrs_ps);
    q->qinfo = httpc_qinfo_dup(&req);
    if (q->on_hdrs) {
        RETHROW((*q->on_hdrs)(q));
    }
    if (conn_close) {
        w->max_queries = 0;
        if (!w->busy) {
            obj_vcall(w, set_busy);
        }
        dlist_for_each_entry_continue(q, &w->query_list, query_link)
        {
            httpc_query_abort(q);
        }
        ob_wipe(&w->ob);
        ob_init(&w->ob);
    }

    return PARSE_OK;
}

static inline int
httpc_flush_data(httpc_t *w, httpc_query_t *q, pstream_t *ps, bool done)
{
    q->received_body_length += ps_len(ps);

    if (w->compressed && !ps_done(ps)) {
        t_scope;
        sb_t zbuf;

        t_sb_init(&zbuf, HTTP_ZLIB_BUFSIZ);
        if (http_zlib_inflate(&w->zs, &w->chunk_length, &zbuf, ps, done)) {
            return PARSE_ERROR;
        }
        RETHROW(q->on_data(q, ps_initsb(&zbuf)));
    } else {
        RETHROW(q->on_data(q, *ps));
        if (w->chunk_length >= 0) {
            w->chunk_length -= ps_len(ps);
        }
        ps->b = ps->b_end;
    }
    return PARSE_OK;
}

static int httpc_parse_body(httpc_t *w, pstream_t *ps)
{
    httpc_query_t *q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
    ssize_t plen = ps_len(ps);

    if (plen >= w->chunk_length && w->chunk_length >= 0) {
        pstream_t tmp = __ps_get_ps(ps, w->chunk_length);

        RETHROW(httpc_flush_data(w, q, &tmp, true));
        return httpc_query_ok(q);
    }
    if (plen >= w->cfg->on_data_threshold) {
        RETHROW(httpc_flush_data(w, q, ps, false));
    }
    return PARSE_MISSING_DATA;
}

static int httpc_parse_chunk_hdr(httpc_t *w, pstream_t *ps)
{
    httpc_query_t *q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
    const char *orig = ps->s;
    pstream_t line, hex;
    uint64_t  len = 0;
    int res;

    res = http_getline(ps, w->cfg->header_line_max, &line);
    if (res) {
        return res;
    }
    http_skipspaces(&line);
    hex = ps_get_span(&line, &ctype_ishexdigit);
    http_skipspaces(&line);
    if (unlikely(ps_len(&line)) != 0 && unlikely(line.s[0] != ';')) {
        return PARSE_ERROR;
    }
    if (unlikely(ps_len(&hex) == 0) || unlikely(ps_len(&hex) > 16)) {
        return PARSE_ERROR;
    }
    for (const char *s = hex.s; s < hex.s_end; s++)
        len = (len << 4) | __str_digit_value[*s + 128];
    w->chunk_length = len;
    w->state = len ? HTTP_PARSER_CHUNK : HTTP_PARSER_CHUNK_TRAILER;
    q->received_body_length += ps->s - orig;
    return PARSE_OK;
}

static int httpc_parse_chunk(httpc_t *w, pstream_t *ps)
{
    httpc_query_t *q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
    ssize_t plen = ps_len(ps);

    assert (w->chunk_length >= 0);
    if (plen >= w->chunk_length + 2) {
        pstream_t tmp = __ps_get_ps(ps, w->chunk_length);

        if (ps_skipstr(ps, "\r\n")) {
            return PARSE_ERROR;
        }
        RETHROW(httpc_flush_data(w, q, &tmp, false));
        w->state = HTTP_PARSER_CHUNK_HDR;
        return PARSE_OK;
    }
    if (plen >= w->cfg->on_data_threshold) {
        RETHROW(httpc_flush_data(w, q, ps, false));
    }
    return PARSE_MISSING_DATA;
}

static int httpc_parse_chunk_trailer(httpc_t *w, pstream_t *ps)
{
    httpc_query_t *q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
    const char *orig = ps->s;
    pstream_t line;

    do {
        int res = http_getline(ps, w->cfg->header_line_max, &line);

        if (res) {
            return res;
        }
    } while (ps_len(&line));

    q->received_body_length += ps->s - orig;
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

int httpc_cfg_tls_init(httpc_cfg_t *cfg, sb_t *err)
{
    assert (cfg->ssl_ctx == NULL);

    cfg->ssl_ctx = ssl_ctx_new_tls(TLS_client_method(),
                                   LSTR_NULL_V, LSTR_NULL_V,
                                   SSL_VERIFY_PEER, NULL, err);
    return cfg->ssl_ctx ? 0 : -1;
}

void httpc_cfg_tls_wipe(httpc_cfg_t *cfg)
{
    if (cfg->ssl_ctx) {
        SSL_CTX_free(cfg->ssl_ctx);
        cfg->ssl_ctx = NULL;
    }
}

int httpc_cfg_tls_add_verify_file(httpc_cfg_t *cfg, lstr_t path)
{
    return SSL_CTX_load_verify_locations(cfg->ssl_ctx, path.s, NULL) == 1
         ? 0 : -1;
}

httpc_cfg_t *httpc_cfg_init(httpc_cfg_t *cfg)
{
    core__httpc_cfg__t iop_cfg;

    p_clear(cfg, 1);

    cfg->httpc_cls = obj_class(httpc);
    iop_init(core__httpc_cfg, &iop_cfg);
    /* Default configuration cannot fail */
    IGNORE(httpc_cfg_from_iop(cfg, &iop_cfg));

    return cfg;
}

int httpc_cfg_from_iop(httpc_cfg_t *cfg, const core__httpc_cfg__t *iop_cfg)
{
    cfg->pipeline_depth    = iop_cfg->pipeline_depth;
    cfg->noact_delay       = iop_cfg->noact_delay;
    cfg->max_queries       = iop_cfg->max_queries;
    cfg->on_data_threshold = iop_cfg->on_data_threshold;
    cfg->header_line_max   = iop_cfg->header_line_max;
    cfg->header_size_max   = iop_cfg->header_size_max;

    if (iop_cfg->tls_on) {
        SB_1k(err);
        char path[PATH_MAX] = "/tmp/tls-cert-XXXXXX";
        int fd;
        int ret;

        if (!iop_cfg->tls_cert.s) {
            logger_error(&_G.logger, "tls: no certificate provided");
            return -1;
        }

        if (httpc_cfg_tls_init(cfg, &err) < 0) {
            logger_error(&_G.logger, "tls: init: %*pM", SB_FMT_ARG(&err));
            return -1;
        }

        if ((fd = mkstemp(path)) < 0) {
            logger_error(&_G.logger, "tls: failed to create a temporary path "
                         "to dump certificate: %m");
            return -1;
        }

        ret = xwrite(fd, iop_cfg->tls_cert.s, iop_cfg->tls_cert.len);
        p_close(&fd);
        if (ret < 0) {
            logger_error(&_G.logger, "tls: failed to dump certificate in "
                         "temporary file `%s`: %m", path);
            unlink(path);
            return -1;
        }

        ret = httpc_cfg_tls_add_verify_file(cfg, LSTR(path));
        unlink(path);
        if (ret < 0) {
            httpc_cfg_tls_wipe(cfg);
            logger_error(&_G.logger, "tls: failed to load certificate");
            return -1;
        }
    }

    return 0;
}

void httpc_cfg_wipe(httpc_cfg_t *cfg)
{
    httpc_cfg_tls_wipe(cfg);
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
    dlist_for_each_entry(httpc_t, w, &lst, pool_link) {
        obj_release(&w);
    }
}

void httpc_pool_wipe(httpc_pool_t *pool, bool wipe_conns)
{
    dlist_t l = DLIST_INIT(l);

    dlist_splice(&l, &pool->busy_list);
    dlist_splice(&l, &pool->ready_list);
    dlist_for_each_entry(httpc_t, w, &l, pool_link) {
        if (wipe_conns) {
            obj_release(&w);
        } else {
            httpc_pool_detach(w);
        }
    }
    lstr_wipe(&pool->name);
    lstr_wipe(&pool->host);
    httpc_cfg_delete(&pool->cfg);
}

void httpc_pool_detach(httpc_t *w)
{
    if (w->pool) {
        w->pool->len--;
        if (w->pool->len_global) {
            (*w->pool->len_global)--;
        }
        dlist_remove(&w->pool_link);
        w->pool = NULL;
    }
}

void httpc_pool_attach(httpc_t *w, httpc_pool_t *pool)
{
    httpc_pool_detach(w);
    w->pool = pool;
    pool->len++;
    if (pool->len_global) {
        (*pool->len_global)++;
    }
    if (w->busy) {
        dlist_add(&pool->busy_list, &w->pool_link);
        if (pool->on_busy) {
            (*pool->on_busy)(pool, w);
        }
    } else {
        dlist_add(&pool->ready_list, &w->pool_link);
        if (pool->on_ready) {
            (*pool->on_ready)(pool, w);
        }
    }
}

httpc_t *httpc_pool_launch(httpc_pool_t *pool)
{
    if (pool->resolve_on_connect) {
        SB_1k(err);
        const char *what = pool->name.s ?: "httpc pool";

        assert(pool->host.s);
        if (addr_resolve_with_err(what, pool->host, &pool->su, &err) < 0) {
            logger_warning(&_G.logger, "%pL", &err);
            return NULL;
        }
    }

    return httpc_connect_as(&pool->su, pool->su_src, pool->cfg, pool);
}

static inline bool httpc_pool_reach_limit(httpc_pool_t *pool)
{
    return (pool->len >= pool->max_len ||
           (pool->len_global && *pool->len_global >= pool->max_len_global));
}

httpc_t *httpc_pool_get(httpc_pool_t *pool)
{
    httpc_t *httpc;

    if (!httpc_pool_has_ready(pool)) {
        if (httpc_pool_reach_limit(pool)) {
            return NULL;
        }
        httpc = RETHROW_P(httpc_pool_launch(pool));
        /* As we are establishing the connection, busy will be true until it
         * is connected. Thus, we will always return NULL here unless you
         * force this flag to false in the on_busy callback for some specific
         * reasons. */
        return httpc->busy ? NULL : httpc;
    }

    httpc = dlist_first_entry(&pool->ready_list, httpc_t, pool_link);
    dlist_move_tail(&pool->ready_list, &httpc->pool_link);
    return httpc;
}

bool httpc_pool_has_ready(httpc_pool_t * nonnull pool)
{
    return !dlist_is_empty(&pool->ready_list);
}

bool httpc_pool_can_query(httpc_pool_t * nonnull pool)
{
    return httpc_pool_has_ready(pool) || !httpc_pool_reach_limit(pool);
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
    if (w->ev) {
        obj_vcall(w, disconnect);
    }
    sb_wipe(&w->ibuf);
    http_zlib_wipe(w);
    ob_wipe(&w->ob);
    httpc_cfg_delete(&w->cfg);
    SSL_free(w->ssl);
}

static void httpc_disconnect(httpc_t *w)
{
    httpc_pool_detach(w);
    el_unregister(&w->ev);
    dlist_for_each(it, &w->query_list) {
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
        if (pool->on_ready) {
            (*pool->on_ready)(pool, w);
        }
    }
}

static void httpc_set_busy(httpc_t *w)
{
    httpc_pool_t *pool = w->pool;

    assert (!w->busy);
    w->busy = true;
    if (pool) {
        dlist_move(&pool->busy_list, &w->pool_link);
        if (pool->on_busy) {
            (*pool->on_busy)(pool, w);
        }
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
    if (!w->busy) {
        obj_vcall(w, set_busy);
    }
    /* let the event loop maybe destroy us later, not now */
    el_fd_set_mask(w->ev, POLLOUT);
}

static void httpc_set_mask(httpc_t *w)
{
    int mask = POLLIN;

    if (!ob_is_empty(&w->ob)) {
        mask |= POLLOUT;
    }
    el_fd_set_mask(w->ev, mask);
}

static int httpc_on_event(el_t evh, int fd, short events, data_t priv)
{
    httpc_t *w = priv.ptr;
    httpc_query_t *q;
    pstream_t ps;
    int res, st = HTTPC_STATUS_INVALID;

    if (events == EL_EVENTS_NOACT) {
        if (!dlist_is_empty(&w->query_list)) {
            q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
            if (q->expect100cont) {
                /* rfc 2616: §8.2.3: the client SHOULD NOT wait
                 * for an indefinite period before sending the request body
                 */
                (*q->on_100cont)(q);
                q->expect100cont = false;
                el_fd_watch_activity(evh, POLLINOUT, w->cfg->noact_delay);
                return 0;
            }
        }
        st = HTTPC_STATUS_TIMEOUT;
        goto close;
    }

    if (events & POLLIN) {
        res = w->ssl ? ssl_sb_read(&w->ibuf, w->ssl, 0)
                     : sb_read(&w->ibuf, fd, 0);
        if (res < 0) {
            if (!ERR_RW_RETRIABLE(errno)) {
                goto close;
            }
            goto write;
        }

        ps = ps_initsb(&w->ibuf);
        if (res == 0) {
            if (w->chunk_length >= 0 || w->state != HTTP_PARSER_BODY) {
                goto close;
            }
            assert (!dlist_is_empty(&w->query_list));
            /* rfc 2616: §4.4: support no Content-Length followed by close */
            w->chunk_length = ps_len(&ps);
        }

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
        if (dlist_is_empty(&w->query_list) && ob_is_empty(&w->ob)) {
            goto close;
        }
    }
  write:
    res = w->ssl ? ob_write_with(&w->ob, fd, ssl_writev, w->ssl)
                 : ob_write(&w->ob, fd);
    if (res < 0 && !ERR_RW_RETRIABLE(errno)) {
        goto close;
    }
    httpc_set_mask(w);
    return 0;

  close:
    httpc_pool_detach(w);
    if (!dlist_is_empty(&w->query_list)) {
        q = dlist_first_entry(&w->query_list, httpc_query_t, query_link);
        if (q->qinfo || st == HTTPC_STATUS_TIMEOUT) {
            httpc_query_on_done(q, st);
        }
    }
    obj_vcall(w, disconnect);
    obj_delete(&w);
    return 0;
}

static void httpc_on_connect_error(httpc_t *w, int errnum)
{
    if (w->pool && w->pool->on_connect_error) {
        (*w->pool->on_connect_error)(w, errnum);
    } else
    if (w->on_connect_error) {
        (*w->on_connect_error)(w, errnum);
    }

    obj_vcall(w, disconnect);
    obj_delete(&w);
}

static int
httpc_tls_handshake(el_t evh, int fd, short events, data_t priv)
{
    httpc_t *w = priv.ptr;
    X509    *cert;

    switch (ssl_do_handshake(w->ssl, evh, fd, NULL)) {
      case SSL_HANDSHAKE_SUCCESS:
        cert = SSL_get_peer_certificate(w->ssl);
        if (unlikely(cert == NULL)) {
            httpc_on_connect_error(w, ECONNREFUSED);
            return -1;
        }
        X509_free(cert);
        httpc_set_mask(w);
        el_fd_set_hook(evh, httpc_on_event);
        obj_vcall(w, set_ready, true);
        break;
      case SSL_HANDSHAKE_PENDING:
        break;
      case SSL_HANDSHAKE_CLOSED:
        httpc_on_connect_error(w, errno);
        break;
      case SSL_HANDSHAKE_ERROR:
        httpc_on_connect_error(w, errno);
        return -1;
    }

    return 0;
}

static int httpc_on_connect(el_t evh, int fd, short events, data_t priv)
{
    httpc_t *w   = priv.ptr;
    int      res;

    if (events == EL_EVENTS_NOACT) {
        httpc_on_connect_error(w, ETIMEDOUT);
        return -1;
    }

    res = socket_connect_status(fd);
    if (res > 0) {
        if (w->cfg->ssl_ctx) {
            w->ssl = SSL_new(w->cfg->ssl_ctx);
            assert (w->ssl);
            SSL_set_fd(w->ssl, fd);
            SSL_set_connect_state(w->ssl);
            el_fd_set_hook(evh, &httpc_tls_handshake);
        } else {
            el_fd_set_hook(evh, httpc_on_event);
            httpc_set_mask(w);
            obj_vcall(w, set_ready, true);
        }
    } else
    if (res < 0) {
        httpc_on_connect_error(w, errno);
    }
    return res;
}

httpc_t *httpc_connect(const sockunion_t *su, httpc_cfg_t *cfg,
                       httpc_pool_t *pool)
{
    return httpc_connect_as(su, NULL, cfg, pool);
}

httpc_t *httpc_connect_as(const sockunion_t *su,
                          const sockunion_t * nullable su_src,
                          httpc_cfg_t *cfg, httpc_pool_t *pool)
{
    httpc_t *w;
    int fd;

    fd = RETHROW_NP(connectx_as(-1, su, 1, su_src, SOCK_STREAM, IPPROTO_TCP,
                                O_NONBLOCK, 0));
    w  = obj_new_of_class(httpc, cfg->httpc_cls);
    w->cfg         = httpc_cfg_retain(cfg);
    w->ev          = el_fd_register(fd, true, POLLOUT, &httpc_on_connect, w);
    w->max_queries = cfg->max_queries;
    el_fd_watch_activity(w->ev, POLLINOUT, w->cfg->noact_delay);
    w->busy        = true;
    if (pool) {
        httpc_pool_attach(w, pool);
    }
    return w;
}

httpc_t *httpc_spawn(int fd, httpc_cfg_t *cfg, httpc_pool_t *pool)
{
    httpc_t *w = obj_new_of_class(httpc, cfg->httpc_cls);

    w->cfg         = httpc_cfg_retain(cfg);
    w->ev          = el_fd_register(fd, true, POLLIN, &httpc_on_event, w);
    w->max_queries = cfg->max_queries;
    el_fd_watch_activity(w->ev, POLLINOUT, w->cfg->noact_delay);
    httpc_set_mask(w);
    if (pool) {
        httpc_pool_attach(w, pool);
    }
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
        if (!w->busy) {
            obj_vcall(w, set_busy);
        }
    }
    if (++w->queries >= w->cfg->pipeline_depth && !w->busy) {
        obj_vcall(w, set_busy);
    }
}

static int httpc_query_on_data_bufferize(httpc_query_t *q, pstream_t ps)
{
    size_t plen = ps_len(&ps);

    if (unlikely(plen + q->payload.len > q->payload_max_size)) {
        return HTTPC_STATUS_TOOLARGE;
    }
    sb_add(&q->payload, ps.s, plen);
    return 0;
}

void httpc_bufferize(httpc_query_t *q, unsigned maxsize)
{
    q->payload_max_size = maxsize;
    q->on_data          = &httpc_query_on_data_bufferize;
}

void httpc_query_start_flags(httpc_query_t *q, http_method_t m,
                             lstr_t host, lstr_t uri, bool httpc_encode_url)
{
    httpc_t  *w  = q->owner;
    outbuf_t *ob = &w->ob;
    int encode_at = 0;

    assert (!q->hdrs_started && !q->hdrs_done);

    ob_add(ob, http_method_str[m].s, http_method_str[m].len);
    ob_adds(ob, " ");
    if (w->cfg->use_proxy) {
        const char *s;

        if (lstr_ascii_istartswith(uri, LSTR("http://"))) {
            uri.s   += 7;
            uri.len -= 7;
            ob_add(ob, "http://", 7);
            s = memchr(uri.s, '/', uri.len);
            encode_at = (s) ? s - uri.s : uri.len;
        } else
        if (lstr_ascii_istartswith(uri, LSTR("https://"))) {
            uri.s   += 8;
            uri.len -= 8;
            ob_add(ob, "https://", 8);
            s = memchr(uri.s, '/', uri.len);
            encode_at = (s) ? s - uri.s : uri.len;
        } else {
            /* Path must be made absolute for HTTP 1.0 proxies */
            ob_addf(ob, "http://%*pM", LSTR_FMT_ARG(host));
            if (unlikely(!uri.len || uri.s[0] != '/')) {
                ob_adds(ob, "/");
            }
        }
    } else {
        assert (!lstr_startswith(uri, LSTR("http://"))
             && !lstr_startswith(uri, LSTR("https://")));
    }
    if (httpc_encode_url) {
        ob_add(ob, uri.s, encode_at);
        ob_add_urlencode(ob, uri.s + encode_at, uri.len - encode_at);
    } else {
        ob_add(ob, uri.s, uri.len);
    }
    ob_addf(ob, " HTTP/1.1\r\n" "Host: %*pM\r\n", LSTR_FMT_ARG(host));
    http_update_date_cache(&date_cache_g, lp_getsec());
    ob_add(ob, date_cache_g.buf, sizeof(date_cache_g.buf) - 1);
    ob_adds(ob, "Accept-Encoding: identity, gzip, deflate\r\n");
    if (w->connection_close) {
        ob_adds(ob, "Connection: close\r\n");
    }
    q->hdrs_started = true;
}

void httpc_query_hdrs_done(httpc_query_t *q, int clen, bool chunked)
{
    outbuf_t *ob = &q->owner->ob;

    assert (!q->hdrs_done);
    q->hdrs_done = true;

    if (q->expect100cont) {
        ob_adds(ob, "Expect: 100-continue\r\n");
    }
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
    if (q->chunked) {
        ob_adds(ob, "\r\n0\r\n\r\n");
    }
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
/* {{{ HTTP2 Framing & Multiplexing Layer */
/* {{{ HTTP2 Constants */

#define PS_NODATA               ps_init(NULL, 0)
#define LSTR_NODATA             LSTR_EMPTY_V
#define HTTP2_STREAM_ID_MASK    0x7fffffff

static const lstr_t http2_client_preface_g =
    LSTR_IMMED("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");

/* standard setting identifier values */
typedef enum setting_id_t {
    HTTP2_ID_HEADER_TABLE_SIZE      = 0x01,
    HTTP2_ID_ENABLE_PUSH            = 0x02,
    HTTP2_ID_MAX_CONCURRENT_STREAMS = 0x03,
    HTTP2_ID_INITIAL_WINDOW_SIZE    = 0x04,
    HTTP2_ID_MAX_FRAME_SIZE         = 0x05,
    HTTP2_ID_MAX_HEADER_LIST_SIZE   = 0x06,
} setting_id_t;

/* special values for stream id field */
enum {
    HTTP2_ID_NO_STREAM              = 0,
    HTTP2_ID_MAX_STREAM             = HTTP2_STREAM_ID_MASK,
};

/* length & size constants */
enum {
    HTTP2_LEN_FRAME_HDR             = 9,
    HTTP2_LEN_NO_PAYLOAD            = 0,
    HTTP2_LEN_PRIORITY_PAYLOAD      = 5,
    HTTP2_LEN_RST_STREAM_PAYLOAD    = 4,
    HTTP2_LEN_SETTINGS_ITEM         = 6,
    HTTP2_LEN_PING_PAYLOAD          = 8,
    HTTP2_LEN_GOAWAY_PAYLOAD_MIN    = 8,
    HTTP2_LEN_WINDOW_UPDATE_PAYLOAD = 4,
    HTTP2_LEN_CONN_WINDOW_SIZE_INIT = (1 << 16) - 1,
    HTTP2_LEN_WINDOW_SIZE_INIT      = (1 << 16) - 1,
    HTTP2_LEN_HDR_TABLE_SIZE_INIT   = 4096,
    HTTP2_LEN_MAX_FRAME_SIZE_INIT   = 1 << 14,
    HTTP2_LEN_MAX_FRAME_SIZE        = (1 << 24) - 1,
    HTTP2_LEN_MAX_SETTINGS_ITEMS    = HTTP2_ID_MAX_HEADER_LIST_SIZE,
    HTTP2_LEN_WINDOW_SIZE_LIMIT     = 0x7fffffff,
};

/* standard frame type values */
typedef enum {
    HTTP2_TYPE_DATA                 = 0x00,
    HTTP2_TYPE_HEADERS              = 0x01,
    HTTP2_TYPE_PRIORITY             = 0x02,
    HTTP2_TYPE_RST_STREAM           = 0x03,
    HTTP2_TYPE_SETTINGS             = 0x04,
    HTTP2_TYPE_PUSH_PROMISE         = 0x05,
    HTTP2_TYPE_PING                 = 0x06,
    HTTP2_TYPE_GOAWAY               = 0x07,
    HTTP2_TYPE_WINDOW_UPDATE        = 0x08,
    HTTP2_TYPE_CONTINUATION         = 0x09,
} frame_type_t;

/* standard frame flag values */
enum {
    HTTP2_FLAG_NONE                 = 0x00,
    HTTP2_FLAG_ACK                  = 0x01,
    HTTP2_FLAG_END_STREAM           = 0x01,
    HTTP2_FLAG_END_HEADERS          = 0x04,
    HTTP2_FLAG_PADDED               = 0x08,
    HTTP2_FLAG_PRIORITY             = 0x20,
};

/* standard error codes */
typedef enum {
    HTTP2_CODE_NO_ERROR             = 0x0,
    HTTP2_CODE_PROTOCOL_ERROR       = 0x1,
    HTTP2_CODE_INTERNAL_ERROR       = 0x2,
    HTTP2_CODE_FLOW_CONTROL_ERROR   = 0x3,
    HTTP2_CODE_SETTINGS_TIMEOUT     = 0x4,
    HTTP2_CODE_STREAM_CLOSED        = 0x5,
    HTTP2_CODE_FRAME_SIZE_ERROR     = 0x6,
    HTTP2_CODE_REFUSED_STREAM       = 0x7,
    HTTP2_CODE_CANCEL               = 0x8,
    HTTP2_CODE_COMPRESSION_ERROR    = 0x9,
    HTTP2_CODE_CONNECT_ERROR        = 0xa,
    HTTP2_CODE_ENHANCE_YOUR_CALM    = 0xb,
    HTTP2_CODE_INADEQUATE_SECURITY  = 0xc,
    HTTP2_CODE_HTTP_1_1_REQUIRED    = 0xd,
} err_code_t;

/* }}} */
/* {{{ Primary Types */

/** Settings of HTTP2 framing layer as per RFC7540/RFC9113 */
typedef struct http2_settings_t {
    uint32_t                    header_table_size;
    uint32_t                    enable_push;
    opt_u32_t                   max_concurrent_streams;
    uint32_t                    initial_window_size;
    uint32_t                    max_frame_size;
    opt_u32_t                   max_header_list_size;
} http2_settings_t;

/* default setting values acc. to RFC7540/RFC9113 */
static http2_settings_t http2_default_settings_g = {
    .header_table_size = HTTP2_LEN_HDR_TABLE_SIZE_INIT,
    .enable_push = 1,
    .max_concurrent_streams = OPT_NONE,
    .initial_window_size = HTTP2_LEN_WINDOW_SIZE_INIT,
    .max_frame_size = HTTP2_LEN_MAX_FRAME_SIZE_INIT,
    .max_header_list_size = OPT_NONE,
};

/* stream state/info flags */
enum {
    STREAM_FLAG_INIT_HDRS = 1 << 0,
    STREAM_FLAG_EOS_RECV  = 1 << 1,
    STREAM_FLAG_EOS_SENT  = 1 << 2,
    STREAM_FLAG_RST_RECV  = 1 << 3,
    STREAM_FLAG_RST_SENT  = 1 << 4,
    STREAM_FLAG_PSH_RECV  = 1 << 5,
    STREAM_FLAG_CLOSED    = 1 << 6,
};

/** XXX: \p http2_stream_t is meant to be passed around by-value. For streams
 * in tracked state, the corresponding values are constructed from \p
 * qm_t(qstream_info) . */

typedef union http2_stream_ctx_t {
    httpc_t *httpc;
    httpd_t *httpd;
} http2_stream_ctx_t;

typedef struct http2_stream_info_t {
    http2_stream_ctx_t  ctx;
    int32_t             recv_wnd;
    int32_t             send_wnd;
    uint8_t             flags;
} http2_stream_info_t;

qm_k32_t(qstream_info, http2_stream_info_t);

typedef struct http2_stream_t {
    uint32_t remove: 1;
    uint32_t id : 31;
    http2_stream_info_t info;
} http2_stream_t;

typedef struct http2_closed_stream_info_t {
    uint32_t    stream_id : 31;
    dlist_t     list_link;
} http2_closed_stream_info_t;

/** info parsed from the frame hdr */
typedef struct http2_frame_info_t {
    uint32_t    len;
    uint32_t    stream_id;
    uint8_t     type;
    uint8_t     flags;
} http2_frame_info_t;

typedef struct http2_client_t http2_client_t;
typedef struct http2_server_ctx_t http2_server_ctx_t;

/** HTTP2 connection object that can be configure as server or client. */
typedef struct http2_conn_t {
    el_t                nonnull ev;
    http2_settings_t    settings;
    http2_settings_t    peer_settings;
    unsigned            refcnt;
    unsigned            id;
    outbuf_t            ob;
    sb_t                ibuf;
    SSL                 * nullable ssl;
    /* hpack compression contexts */
    hpack_enc_dtbl_t    enc;
    hpack_dec_dtbl_t    dec;
    /* tracked streams */
    qm_t(qstream_info)  stream_info;
    dlist_t             closed_stream_info;
    uint32_t            client_streams;
    uint32_t            server_streams;
    uint32_t            closed_streams_info_cnt;
    /* backstream contexts */
    union {
        http2_client_t *nullable client_ctx;
        http2_server_ctx_t *nullable server_ctx;
    };
    /* flow control */
    int32_t             recv_wnd;
    int32_t             send_wnd;
    /* frame parser */
    http2_frame_info_t  frame;
    unsigned            cont_chunk;
    uint8_t             state;
    /* connection flags */
    bool                is_client : 1;
    bool                is_settings_acked : 1;
    bool                is_conn_err_recv: 1;
    bool                is_conn_err_sent: 1;
    bool                is_shtdwn_recv: 1;
    bool                is_shtdwn_sent: 1;
    bool                is_shtdwn_soon_recv: 1;
    bool                is_shtdwn_soon_sent: 1;
    bool                is_shtdwn_commanded : 1;
} http2_conn_t;

/** Get effective HTTP2 settings */
static http2_settings_t http2_get_settings(http2_conn_t *w)
{
    return likely(w->is_settings_acked) ? w->settings
                                        : http2_default_settings_g;
}

typedef struct http2_header_info_t {
    bool invalid;
    lstr_t scheme;
    lstr_t method;
    lstr_t path;
    lstr_t authority;
    lstr_t status;
    lstr_t content_length;
} http2_header_info_t;

/* }}}*/
/* {{{ Connection-Level & Stream-Level Logging */

/* TODO: add additional conn-related info to the log message */
#define http2_conn_log(/* const http_conn_t* */ w, /* int */ level,          \
                       /* const char* */ fmt, ...)                           \
    logger_log(&_G.logger, level, fmt, ##__VA_ARGS__)

#define http2_conn_trace(w, level, fmt, ...)                                 \
    http2_conn_log(w, LOG_TRACE + (level), "[h2c %u] " fmt, (w)->id,         \
                   ##__VA_ARGS__)

/* TODO: add additional stream-related info to the log message */
#define http2_stream_log(/* const http_conn_t* */ w,                         \
                         /* const stream_t* */ stream, /* int */ level,      \
                         /* const char* */ fmt, ...)                         \
    logger_log(&_G.logger, level, "[h2c %u, sid %d] " fmt, (w)->id,          \
               (stream)->id, ##__VA_ARGS__)

#define http2_stream_trace(w, stream, level, fmt, ...)                       \
    http2_stream_log(w, stream, LOG_TRACE + (level), fmt, ##__VA_ARGS__)

/* }}} */
/* {{{ Connection Management */

static http2_conn_t *http2_conn_init(http2_conn_t *w)
{
    p_clear(w, 1);
    w->id = ++_G.http2_conn_count;
    sb_init(&w->ibuf);
    ob_init(&w->ob);
    dlist_init(&w->closed_stream_info);
    qm_init(qstream_info, &w->stream_info);
    w->peer_settings = http2_default_settings_g;
    w->recv_wnd = HTTP2_LEN_CONN_WINDOW_SIZE_INIT;
    w->send_wnd = HTTP2_LEN_CONN_WINDOW_SIZE_INIT;
    hpack_enc_dtbl_init(&w->enc);
    hpack_dec_dtbl_init(&w->dec);
    hpack_enc_dtbl_init_settings(&w->enc, w->peer_settings.header_table_size);
    hpack_dec_dtbl_init_settings(&w->dec,
                                 http2_get_settings(w).header_table_size);
    return w;
}

static void http2_conn_wipe(http2_conn_t *w)
{
    hpack_dec_dtbl_wipe(&w->dec);
    hpack_enc_dtbl_wipe(&w->enc);
    ob_wipe(&w->ob);
    sb_wipe(&w->ibuf);
    qm_wipe(qstream_info, &w->stream_info);
    assert(dlist_is_empty(&w->closed_stream_info));
    el_unregister(&w->ev);
}

DO_REFCNT(http2_conn_t, http2_conn)

/** Return the maximum id of the (non-idle) server stream or 0 if none. */
static uint32_t http2_conn_max_server_stream_id(http2_conn_t *w)
{
    /* Server streams have even ids: 2, 4, 6, and so on. Server streams with
     * ids superior than this http2_conn_max_server_stream_id are idle,
     * otherwise, they are non idle (either active or closed). So, the next
     * available idle server stream is (http2_conn_max_server_stream_id + 2).
     * Note, that initiating a stream above this value, i.e., skipping some
     * ids is possible and implies closing the streams with the skipped ids.
     * So, this threshold is tracked using the number of streams (non-idle)
     * sor far. So, 0 server stream => 0 max server stream id (the next idle
     * stream is 2), 1 server stream => 2 max server stream id (the next idle
     * stream is 4), and so on.
     */
    return 2 * w->server_streams;
}

/** Return the maximum id of the (non-idle) client stream or 0 if none. */
static uint32_t http2_conn_max_client_stream_id(http2_conn_t *w)
{
    /* Client streams have odd ids: 1, 3, 5, and so on. Client streams with
     * ids superior than this http2_conn_max_client_stream_id are idle,
     * otherwise, they are non idle (either active or closed). So, the next
     * available idle client stream is (http2_conn_max_client_stream_id + 2)
     * except for client_streams = 0 where the next idle stream is 1. Note,
     * that initiating a stream above this value, i.e., skipping some ids is
     * possible and implies closing the streams with the skipped ids. So, this
     * threshold is tracked using the number of streams (non-idle) sor far.
     * So, 0 client stream => max client stream id = 0 (the next idle stream
     * is 1), 1 client stream => max client stream id = 1 (the next idle
     * stream is 3), 2 client streams => max client stream id = 3 (the next
     * idle stream is 5) and so on.
     */
    return 2 * w->client_streams - !!w->client_streams;
}

/** Return the maximum id of the (non-idle) peer stream or 0 if none. */
static uint32_t http2_conn_max_peer_stream_id(http2_conn_t *w)
{
    return w->is_client ? http2_conn_max_server_stream_id(w)
                        : http2_conn_max_client_stream_id(w);
}

/* }}}*/
/* {{{ Send Buffer Framing */

typedef struct http2_frame_hdr_t {
    be32_t len : 24;
    uint8_t type;
    uint8_t flags;
    be32_t stream_id;
} __attribute__((packed)) http2_frame_hdr_t;

static void
http2_conn_send_common_hdr(http2_conn_t *w, unsigned len, uint8_t type,
                           uint8_t flags, uint32_t stream_id)
{
    http2_frame_hdr_t hdr;

    STATIC_ASSERT(sizeof(hdr) == HTTP2_LEN_FRAME_HDR);
    STATIC_ASSERT(HTTP2_LEN_MAX_FRAME_SIZE < (1 << 24));
    assert(len <= HTTP2_LEN_MAX_FRAME_SIZE);
    hdr.len = cpu_to_be32(len);
    hdr.type = type;
    hdr.flags = flags;
    hdr.stream_id = cpu_to_be32(stream_id);
    ob_add(&w->ob, &hdr, sizeof(hdr));
}

__unused__
static void http2_conn_send_preface(http2_conn_t *w)
{
    if (w->is_client) {
        OB_WRAP(sb_add_lstr, &w->ob, http2_client_preface_g);
    }
}

typedef struct {
    uint16_t id;
    uint32_t val;
} setting_item_t;

qvector_t(qsettings, setting_item_t);

__unused__
static void http2_conn_send_init_settings(http2_conn_t *w)
{
    t_scope;
    http2_settings_t defaults;
    http2_settings_t init_settings;
    qv_t(qsettings) items;

#define STNG_ITEM(id_, val_)                                                 \
    (setting_item_t)                                                         \
    {                                                                        \
        .id = HTTP2_ID_##id_, .val = init_settings.val_                      \
    }

#define STNG_ITEM_OPT(id_, val_)                                             \
    (setting_item_t){.id = HTTP2_ID_##id_, .val = OPT_VAL(init_settings.val_)}

    t_qv_init(&items, HTTP2_LEN_MAX_SETTINGS_ITEMS);
    defaults = http2_default_settings_g;
    init_settings = w->settings;
    if (init_settings.header_table_size != defaults.header_table_size) {
        qv_append(&items, STNG_ITEM(HEADER_TABLE_SIZE, header_table_size));
    }
    if (w->is_client && init_settings.enable_push != defaults.enable_push) {
        qv_append(&items, STNG_ITEM(ENABLE_PUSH, enable_push));
    }
    if (OPT_ISSET(init_settings.max_concurrent_streams) &&
        !OPT_EQUAL(init_settings.max_concurrent_streams,
                   defaults.max_concurrent_streams))
    {
        qv_append(&items, STNG_ITEM_OPT(MAX_CONCURRENT_STREAMS,
                                        max_concurrent_streams));
    }
    if (init_settings.initial_window_size != defaults.initial_window_size) {
        qv_append(&items,
                  STNG_ITEM(INITIAL_WINDOW_SIZE, initial_window_size));
    }
    if (init_settings.max_frame_size != defaults.max_frame_size) {
        qv_append(&items, STNG_ITEM(MAX_FRAME_SIZE, max_frame_size));
    }
    if (OPT_ISSET(init_settings.max_header_list_size) &&
        !OPT_EQUAL(init_settings.max_header_list_size,
                   defaults.max_header_list_size))
    {
        qv_append(&items, STNG_ITEM_OPT(MAX_HEADER_LIST_SIZE,
                                        max_header_list_size));
    }
    assert(items.len <= HTTP2_LEN_MAX_SETTINGS_ITEMS);
    http2_conn_send_common_hdr(w, HTTP2_LEN_SETTINGS_ITEM * items.len,
                               HTTP2_TYPE_SETTINGS, HTTP2_FLAG_NONE,
                               HTTP2_ID_NO_STREAM);
    tab_for_each_ptr(item, &items) {
        OB_WRAP(sb_add_be16, &w->ob, item->id);
        OB_WRAP(sb_add_be32, &w->ob, item->val);
    }

#undef STNG_ITEM_OPT
#undef STNG_ITEM
}

typedef struct http2_min_goaway_payload_t {
    be32_t last_stream_id;
    be32_t error_code;
} http2_min_goaway_payload_t;

static void http2_conn_send_goaway(http2_conn_t *w, uint32_t last_stream_id,
                                   uint32_t error_code, lstr_t debug)
{
    http2_min_goaway_payload_t payload;
    int len = sizeof(payload) + debug.len;

    STATIC_ASSERT(sizeof(payload) == HTTP2_LEN_GOAWAY_PAYLOAD_MIN);
    assert(last_stream_id <= HTTP2_ID_MAX_STREAM);
    payload.last_stream_id = cpu_to_be32(last_stream_id);
    payload.error_code = cpu_to_be32(error_code);
    http2_conn_send_common_hdr(w, len, HTTP2_TYPE_GOAWAY, HTTP2_FLAG_NONE,
                               HTTP2_ID_NO_STREAM);
    ob_add(&w->ob, &payload, sizeof(payload));
    ob_add(&w->ob, debug.data, debug.len);
}

/** Send data block as 0 or more data frames. */
__unused__
static void http2_conn_send_data_block(http2_conn_t *w, uint32_t stream_id,
                                       pstream_t blk, bool end_stream)
{
    pstream_t chunk;
    uint8_t flags;
    unsigned len;

    if (ps_done(&blk) && !end_stream) {
        return;
    }
    /* HTTP2_LEN_MAX_FRAME_SIZE_INIT is also the minimum possible value so
     * peer must always accept frames of this size. */
    assert (w->send_wnd >= (int) ps_len(&blk));
    w->send_wnd -= ps_len(&blk);
    do {
        len = MIN(ps_len(&blk), HTTP2_LEN_MAX_FRAME_SIZE_INIT);
        chunk = __ps_get_ps(&blk, len);
        flags = ps_done(&blk) && end_stream ? HTTP2_FLAG_END_STREAM : 0;
        http2_conn_send_common_hdr(w, len, HTTP2_TYPE_DATA, flags, stream_id);
        OB_WRAP(sb_add_ps, &w->ob, chunk);
    } while (!ps_done(&blk));
}

/** Send header block as 1 header frame plus 0 or more continuation frames. */
__unused__
static void http2_conn_send_headers_block(http2_conn_t *w, uint32_t stream_id,
                                          pstream_t blk, bool end_stream)
{
    pstream_t chunk;
    uint8_t type;
    uint8_t flags;
    unsigned len;

    assert(!ps_done(&blk));
    /* HTTP2_LEN_MAX_FRAME_SIZE_INIT is also the minimum possible value so
     * peer must always accept frames of this size. */
    type = HTTP2_TYPE_HEADERS;
    flags = end_stream ? HTTP2_FLAG_END_STREAM : HTTP2_FLAG_NONE;
    do {
        len = MIN(ps_len(&blk), HTTP2_LEN_MAX_FRAME_SIZE_INIT);
        chunk = __ps_get_ps(&blk, len);
        flags |= ps_done(&blk) ? HTTP2_FLAG_END_HEADERS : 0;
        http2_conn_send_common_hdr(w, len, type, flags, stream_id);
        OB_WRAP(sb_add_ps, &w->ob, chunk);
        type = HTTP2_TYPE_CONTINUATION;
        flags = HTTP2_FLAG_NONE;
    } while (!ps_done(&blk));
}

__unused__
static void http2_conn_send_rst_stream(http2_conn_t *w, uint32_t stream_id,
                                       uint32_t error_code)
{
    assert(stream_id);
    http2_conn_send_common_hdr(w, HTTP2_LEN_RST_STREAM_PAYLOAD,
                               HTTP2_TYPE_RST_STREAM, HTTP2_FLAG_NONE,
                               stream_id);
    OB_WRAP(sb_add_be32, &w->ob, error_code);
}

__unused__
static void http2_conn_send_window_update(http2_conn_t *w, uint32_t stream_id,
                                          uint32_t incr)
{
    assert(incr > 0 && incr <= 0x7fffffff);
    http2_conn_send_common_hdr(w, HTTP2_LEN_WINDOW_UPDATE_PAYLOAD,
                               HTTP2_TYPE_WINDOW_UPDATE, HTTP2_FLAG_NONE,
                               stream_id);
    OB_WRAP(sb_add_be32, &w->ob, incr);
}

__unused__
static void
http2_conn_send_shutdown(http2_conn_t *w, lstr_t debug)
{
    uint32_t stream_id = http2_conn_max_peer_stream_id(w);

    assert(!w->is_shtdwn_sent);
    w->is_shtdwn_sent = true;
    http2_conn_send_goaway(w, stream_id, HTTP2_CODE_NO_ERROR, debug);
}

__unused__
static int
http2_conn_send_error(http2_conn_t *w, uint32_t error_code, lstr_t debug)
{
    uint32_t stream_id = http2_conn_max_peer_stream_id(w);

    assert(error_code != HTTP2_CODE_NO_ERROR);
    assert(!w->is_conn_err_sent);
    w->is_conn_err_sent = true;
    http2_conn_send_goaway(w, stream_id, error_code, debug);

    return -1;
}

/* }}} */
/* {{{ Stream Management */

static bool http2_stream_id_is_server(uint32_t stream_id)
{
    assert(stream_id);
    assert(stream_id <= (uint32_t)HTTP2_ID_MAX_STREAM);
    return stream_id % 2 == 0;
}

static bool http2_stream_id_is_client(uint32_t stream_id)
{
    return !http2_stream_id_is_server(stream_id);
}

/** Return the number of streams (of the same class) upto to \p stream_id. */
static uint32_t http2_get_nb_streams_upto(uint32_t stream_id)
{
    assert(stream_id);
    assert(stream_id <= (uint32_t)HTTP2_ID_MAX_STREAM);

    return DIV_ROUND_UP(stream_id, 2);
}

/** Return the stream (info) with id = \p stream_id */
__unused__
static http2_stream_t http2_stream_get(http2_conn_t *w, uint32_t stream_id)
{
    http2_stream_t stream = {.id = stream_id};
    http2_stream_info_t *info;
    uint32_t nb_streams;

    nb_streams = http2_stream_id_is_client(stream_id) ? w->client_streams
                                                      : w->server_streams;
    if (http2_get_nb_streams_upto(stream_id) > nb_streams) {
        /* stream is idle. */
        return stream;
    }
    info = qm_get_def_p(qstream_info, &w->stream_info, stream_id, NULL);
    if (info) {
        /* stream is non_idle. */
        stream.info = *info;
    } else {
        /* stream is closed<untracked state>. */
        stream.info.flags = STREAM_FLAG_CLOSED;
    }
    return stream;
}

/** Get the next idle (available) stream id. */
__unused__
static uint32_t http2_stream_get_idle(http2_conn_t *w)
{
    uint32_t stream_id;

    /* XXX: only relevant on the client side since we don't support creating
     * server (pushed) streams (yet!). */
    assert(w->is_client);
    stream_id = 2 * (uint32_t)w->client_streams + 1;
    assert(stream_id <= (uint32_t)HTTP2_ID_MAX_STREAM);
    return stream_id;
}

static void
http2_closed_stream_info_create(http2_conn_t *w, const http2_stream_t *stream)
{
    http2_closed_stream_info_t *info = p_new(http2_closed_stream_info_t, 1);

    info->stream_id = stream->id;
    dlist_add_tail(&w->closed_stream_info, &info->list_link);
    w->closed_streams_info_cnt++;
}

__unused__
static void
http2_stream_do_update_info(http2_conn_t *w, http2_stream_t *stream)
{
    unsigned flags = stream->info.flags;

    if (stream->remove) {
        qm_del_key(qstream_info, &w->stream_info, stream->id);
    } else {
        assert(flags && !(flags & STREAM_FLAG_CLOSED));
        qm_replace(qstream_info, &w->stream_info, stream->id, stream->info);
    }
}

__unused__
static void http2_stream_do_on_events(http2_conn_t *w, http2_stream_t *stream,
                                      unsigned events)
{
    unsigned flags = stream->info.flags;

    assert(events);
    assert(!(flags & STREAM_FLAG_CLOSED));
    assert(!(flags & events));
    if (!flags) {
        /* Idle stream */
        uint32_t nb_streams;
        uint32_t new_nb_streams;

        nb_streams = http2_stream_id_is_client(stream->id)
                         ? w->client_streams
                         : w->server_streams;
        new_nb_streams = http2_get_nb_streams_upto(stream->id);
        assert(new_nb_streams > nb_streams);
        if (events == STREAM_FLAG_INIT_HDRS) {
            http2_stream_trace(w, stream, 0, "opened");
        } else if (events == (STREAM_FLAG_INIT_HDRS | STREAM_FLAG_EOS_RECV)) {
            http2_stream_trace(w, stream, 0, "half closed (remote)");
        } else if (events == (STREAM_FLAG_INIT_HDRS | STREAM_FLAG_EOS_SENT)) {
            http2_stream_trace(w, stream, 0, "half closed (local)");
        } else if (events == (STREAM_FLAG_PSH_RECV | STREAM_FLAG_RST_SENT)) {
            assert(w->is_client && !stream->id);
            http2_stream_trace(w, stream, 0, "closed [pushed, reset sent]");
        } else {
            assert(0 && "invalid events on idle stream");
        }
        /* RFC7541(RFC9113) § 5.1.1. Stream Identifiers */
        if (http2_stream_id_is_client(stream->id)) {
            w->client_streams = new_nb_streams;
        } else {
            w->server_streams = new_nb_streams;
        }
        stream->info.flags = events;
        stream->info.recv_wnd = http2_get_settings(w).initial_window_size;
        stream->info.send_wnd = w->peer_settings.initial_window_size;
        return;
    }
    if (events == STREAM_FLAG_EOS_RECV) {
        if (flags & STREAM_FLAG_EOS_SENT) {
            http2_stream_trace(w, stream, 0, "stream closed [eos recv]");
            stream->remove = true;
        } else {
            http2_stream_trace(w, stream, 0, "stream half closed (remote)");
        }
    } else if (events == STREAM_FLAG_EOS_SENT) {
        if (flags & STREAM_FLAG_EOS_RECV) {
            http2_stream_trace(w, stream, 0, "stream closed [eos sent]");
            http2_closed_stream_info_create(w, stream);
        } else {
            http2_stream_trace(w, stream, 0, "stream half closed (local)");
        }
    } else if (events == STREAM_FLAG_RST_RECV) {
        http2_stream_trace(w, stream, 0, "stream closed [reset recv]");
        stream->remove = true;
    } else if (events == STREAM_FLAG_RST_SENT) {
        http2_stream_trace(w, stream, 0, "stream closed [reset sent]");
        http2_closed_stream_info_create(w, stream);
    } else {
        assert(0 && "unexpected stream state transition");
    }
    stream->info.flags = flags | events;
}

/* }}}*/
/* {{{ Headers Packing/Unpacking (HPACK) */

/** Decode a header block.
 *
 * \param res: decoded headers info.
 * \return 0 if decoding succeed, -1 otherwise.
 *
 * XXX: Basic header validation against RFC 9113 "§8.3. HTTP Control Data" is
 * done and conveyed in \p res->invalid.
 */
__unused__
static int
t_http2_conn_decode_header_block(http2_conn_t *w, pstream_t in,
                                 http2_header_info_t *res, sb_t *buf)
{
    hpack_dec_dtbl_t *dec = &w->dec;
    http2_header_info_t info = {0};
    int nb_pseudo_hdrs = 0;
    bool regular_hdrs_seen = false;

    while (RETHROW(hpack_decoder_read_dts_update(dec, &in))) {
        /* read dynamic table size updates. */
    }
    while (!ps_done(&in)) {
        hpack_xhdr_t xhdr;
        int len;
        int keylen;
        byte *out;
        lstr_t key;
        lstr_t val;

        len = RETHROW(hpack_decoder_extract_hdr(dec, &in, &xhdr));
        out = (byte *)sb_grow(buf, len);
        /* XXX: Decoded header is unpacked into the following format:
         * <DECODED_KEY> + ": " + <DECODED_VALUE> + "\r\n".
         */
        len = RETHROW(hpack_decoder_write_hdr(dec, &xhdr, out, &keylen));
        key = LSTR_DATA_V(out, keylen);
        val = LSTR_DATA_V(out + keylen + 2, len - keylen - 4);
        http2_conn_trace(w, 0, "%*pM: %*pM", LSTR_FMT_ARG(key),
                         LSTR_FMT_ARG(val));
        THROW_ERR_IF(keylen < 1);
        if (info.invalid) {
            /* XXX: must continue the decoding to keep the HPACK context (the
             * dynamic table) synchroized between the two parties. */
            continue;
        }
        /* TODO: optimize this search */
        if (unlikely(key.s[0] == ':')) {
            lstr_t *matched_phdr = NULL;

            if (regular_hdrs_seen) {
                /* Regular headers must follow the pseudo headers. */
                info.invalid = true;
            }
            nb_pseudo_hdrs++;
            if (lstr_equal(key, LSTR_IMMED_V(":scheme"))) {
                matched_phdr = &info.scheme;
            } else if (lstr_equal(key, LSTR_IMMED_V(":method"))) {
                matched_phdr = &info.method;
            } else if (lstr_equal(key, LSTR_IMMED_V(":authority"))) {
                matched_phdr = &info.authority;
            } else if (lstr_equal(key, LSTR_IMMED_V(":path"))) {
                matched_phdr = &info.path;
            } else if (lstr_equal(key, LSTR_IMMED_V(":status"))) {
                matched_phdr = &info.status;
            }
            if (matched_phdr) {
                if (!matched_phdr->s) {
                    *matched_phdr = t_lstr_dup(val);
                } else {
                    /* duplicated pseudo header */
                    info.invalid = true;
                }
            } else {
                /* unkown pseudo header */
                info.invalid = true;
            }
        } else {
            regular_hdrs_seen = true;
            if (lstr_ascii_iequal(key, LSTR_IMMED_V("content-length"))) {
                info.content_length = val;
            }
            buf->len += len;
        }
    }
    sb_set_trailing0(buf);
    /* Basic validation according to RFC9113 §8.3. */
    switch (nb_pseudo_hdrs) {
    case 0:
        /* 0 pseudo-hdr is okay for trailer headers. */
        break;
    case 1:
        if (!info.status.s) {
            /* 1 pseudo-hdr: it has to be status for response hdr. */
            info.invalid = true;
        }
        break;
    case 4:
        if (!info.authority.s) {
            /* 4 pseudo-hdrs has to be:
             * authority, method, scheme, path. */
            info.invalid = true;
        }
        /* FALLTHROUGH */
    case 3:
        if (!info.method.s || !info.scheme.s || !info.path.s) {
            /* 3 pseudo-hdrs has to be: method, scheme, path. */
            info.invalid = true;
        }
        break;
    default:
        info.invalid = true;
        break;
    }
    *res = info;
    return 0;
}

__unused__
static void http2_headerlines_get_next_hdr(pstream_t *headerlines,
                                           lstr_t *key, lstr_t *val)
{
    pstream_t line = PS_NODATA;
    pstream_t ps = PS_NODATA;
    int rc;

    assert(!ps_done(headerlines));
    rc = ps_get_ps_upto_str_and_skip(headerlines, "\r\n", &line);
    assert(rc >= 0 && !ps_done(&line));
    rc = ps_get_ps_chr_and_skip(&line, ':', &ps);
    assert(rc >= 0);
    ps_trim(&ps);
    assert(!ps_done(&ps));
    *key = LSTR_PS_V(&ps);
    ps_trim(&line);
    assert(!ps_done(&line));
    *val = LSTR_PS_V(&line);
}

__unused__
static void http2_conn_pack_single_hdr(http2_conn_t *w, lstr_t key,
                                       lstr_t val, sb_t *out_)
{
    hpack_enc_dtbl_t *enc = &w->enc;
    int len;
    int buflen;
    byte *out;

    buflen = hpack_buflen_to_write_hdr(key, val, 0);
    out = (byte *)sb_grow(out_, buflen);
    len = hpack_encoder_write_hdr(enc, key, val, 0, 0, 0, out);
    assert(len > 0);
    assert(len <= buflen);
    __sb_fixlen(out_, out_->len + len);
}

/* }}} */
/* {{{ Streaming API */

__unused__
static void
http2_stream_on_headers(http2_conn_t *w, http2_stream_t stream,
                        http2_stream_ctx_t ctx, http2_header_info_t *info,
                        pstream_t headerlines, bool eos)
{
    if (w->is_client) {
        /* TODO */
    } else {
        /* TODO */
    }
}

__unused__
static void
http2_stream_on_data(http2_conn_t *w, http2_stream_t stream,
                     http2_stream_ctx_t ctx, pstream_t data, bool eos)
{
    if (w->is_client) {
        /* TODO */
    } else {
        /* TODO */
    }
}

__unused__
static void http2_stream_on_reset(http2_conn_t *w, http2_stream_t stream,
                                  http2_stream_ctx_t ctx, bool remote)
{
    if (w->is_client) {
        /* TODO */
    } else {
        /* TODO */
    }
}

__unused__
static void http2_conn_on_streams_can_write(http2_conn_t *w)
{
    if (w->is_client) {
        /* TODO */
    } else {
        /* TODO */
    }
}

__unused__
static void http2_conn_on_close(http2_conn_t *w)
{
    if (w->is_client) {
        /* TODO */
    } else {
        /* TODO */
    }
}

__unused__
static bool
http2_is_valid_request_hdr_to_send(lstr_t key_, lstr_t val, int *clen)
{
    pstream_t key = ps_initlstr(&key_);
    int rc;

    switch (http_wkhdr_from_ps(key)) {
    case HTTP_WKHDR_CONNECTION:
    case HTTP_WKHDR_TRANSFER_ENCODING:

        return false;
    case HTTP_WKHDR_CONTENT_LENGTH:
        rc = lstr_to_int(val, clen);
        assert(!rc);
        break;
    default:
        break;
    }
    return true;
}

__unused__
static void
http2_stream_send_request_headers(http2_conn_t *w, http2_stream_t *stream,
                                  lstr_t method, lstr_t scheme, lstr_t path,
                                  lstr_t authority, pstream_t headerlines,
                                  int *clen)
{
    SB_1k(out);
    unsigned events;
    bool eos;

    *clen = -1;
    http2_conn_pack_single_hdr(w, LSTR_IMMED_V(":method"), method, &out);
    http2_conn_pack_single_hdr(w, LSTR_IMMED_V(":scheme"), scheme, &out);
    http2_conn_pack_single_hdr(w, LSTR_IMMED_V(":path"), path, &out);
    if (authority.s) {
        http2_conn_pack_single_hdr(w, LSTR_IMMED_V(":authority"), authority,
                                   &out);
    }
    while (!ps_done(&headerlines)) {
        lstr_t key;
        lstr_t val;

        http2_headerlines_get_next_hdr(&headerlines, &key, &val);
        if (!http2_is_valid_request_hdr_to_send(key, val, clen)) {
            continue;
        }
        http2_conn_pack_single_hdr(w, key, val, &out);
    }
    eos = (*clen == 0);
    http2_conn_send_headers_block(w, stream->id, ps_initsb(&out), eos);
    events = STREAM_FLAG_INIT_HDRS | (eos ? STREAM_FLAG_EOS_SENT : 0);
    http2_stream_do_on_events(w, stream, events);
    http2_stream_do_update_info(w, stream);
}

__unused__
static void http2_stream_send_data(http2_conn_t *w, http2_stream_t *stream,
                                   pstream_t data, bool eos)
{
    int len = ps_len(&data);

    assert(stream->info.send_wnd >= len);
    stream->info.send_wnd -= len;
    http2_conn_send_data_block(w, stream->id, data, eos);
    if (eos) {
        http2_stream_do_on_events(w, stream, STREAM_FLAG_EOS_SENT);
    }
    http2_stream_do_update_info(w, stream);
}

#define http2_stream_send_reset(w, stream, fmt, ...)                         \
    do {                                                                     \
        http2_stream_error(w, stream, PROTOCOL_ERROR, fmt, ##__VA_ARGS__);   \
        http2_stream_do_update_info(w, stream);                              \
    } while (0)

#define http2_stream_send_reset_cancel(w, stream, fmt, ...)                  \
    do {                                                                     \
        http2_stream_error(w, stream, CANCEL, fmt, ##__VA_ARGS__);           \
        http2_stream_do_update_info(w, stream);                              \
    } while (0)

/* }}} */
/* }}} */
/* {{{ HTTP Module */

static int http_initialize(void *arg)
{
    return 0;
}

static int http_shutdown(void)
{
    return 0;
}

MODULE_BEGIN(http)
    MODULE_DEPENDS_ON(ssl);
MODULE_END()

/* }}} */
/* Tests {{{ */

#include <lib-common/z.h>

static bool has_reply_g;
static http_code_t code_g;
static sb_t body_g;
static httpc_query_t zquery_g;
static el_t zel_server_g;
static el_t zel_client_g;
static httpc_cfg_t zcfg_g;
static httpc_status_t zstatus_g;
static httpc_t *zhttpc_g;
static sb_t zquery_sb_g;

static int z_reply_100(el_t el, int fd, short mask, data_t data)
{
    SB_1k(buf);

    if (sb_read(&buf, fd, 1000) > 0) {
        char reply[] = "HTTP/1.1 100 Continue\r\n\r\n"
                       "HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\n"
                       "Coucou";

        IGNORE(xwrite(fd, reply, sizeof(reply) - 1));
    }
    return 0;
}

static int z_reply_keep(el_t el, int fd, short mask, data_t data)
{
    sb_reset(&zquery_sb_g);
    if (sb_read(&zquery_sb_g, fd, BUFSIZ) > 0) {
        char reply[] = "HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\n"
                       "Coucou";

        IGNORE(xwrite(fd, reply, sizeof(reply) - 1));
    }
    return 0;
}

static int z_reply_gzip_empty(el_t el, int fd, short mask, data_t data)
{
    SB_1k(buf);

    if (sb_read(&buf, fd, 1000) > 0) {
        char reply[] = "HTTP/1.1 202 Accepted\r\n"
                       "Content-Encoding: gzip\r\n"
                       "Content-Length: 0\r\n"
                       "\r\n";

        IGNORE(xwrite(fd, reply, sizeof(reply) - 1));
    }
    return 0;
}

static int z_reply_close_without_content_length(el_t el, int fd, short mask,
                                                data_t data)
{
    SB_1k(buf);

    if (sb_read(&buf, fd, 1000) > 0) {
        char reply[] = "HTTP/1.1 200 OK\r\n\r\n"
                       "Plop";
        char s[8192];

        IGNORE(xwrite(fd, reply, sizeof(reply) - 1));
        fd_set_features(fd, O_NONBLOCK);
        for (int i = 0; i < 4096; i++) {
            ssize_t len = ssizeof(s);
            char *ptr = s;

            memset(s, 'a' + (i % 26), sizeof(s));
            while (len > 0) {
                ssize_t res;

                if ((res = write(fd, ptr, len)) <= 0) {
                    if (res < 0 && !ERR_RW_RETRIABLE(errno)) {
                        logger_panic(&_G.logger, "write error: %m");
                    }
                    el_fd_loop(zhttpc_g->ev, 10, EV_FDLOOP_HANDLE_TIMERS);
                    continue;
                }
                ptr += res;
                len -= res;
            }
        }
        el_unregister(&zel_client_g);
    }
    return 0;
}

static int z_reply_no_content(el_t el, int fd, short mask, data_t data)
{
    SB_1k(buf);

    if (sb_read(&buf, fd, 1000) > 0) {
        char reply[] = "HTTP/1.1 204 No Content\r\n\r\n";

        IGNORE(xwrite(fd, reply, sizeof(reply) - 1));
    }
    return 0;
}

static int z_accept(el_t el, int fd, short mask, data_t data)
{
    int (* query_cb)(el_t, int, short, data_t) = data.ptr;
    int client = acceptx(fd, 0);

    if (client >= 0) {
        zel_client_g = el_fd_register(client, true, POLLIN, query_cb, NULL);
    }
    return 0;
}

static int z_query_on_hdrs(httpc_query_t *q)
{
    code_g = q->qinfo->code;
    return 0;
}

static int z_query_on_data(httpc_query_t *q, pstream_t ps)
{
    sb_add(&body_g, ps.s, ps_len(&ps));
    return 0;
}

static void z_query_on_done(httpc_query_t *q, httpc_status_t status)
{
    has_reply_g = true;
    zstatus_g = status;
}

enum z_query_flags {
    Z_QUERY_USE_PROXY = (1 << 0),
};

static int z_query_setup(int (* query_cb)(el_t, int, short, el_data_t),
                         enum z_query_flags flags, lstr_t host, lstr_t uri)
{
    sockunion_t su;
    int server;

    zstatus_g = HTTPC_STATUS_ABORT;
    has_reply_g = false;
    code_g = HTTP_CODE_INTERNAL_SERVER_ERROR;
    sb_init(&body_g);
    sb_init(&zquery_sb_g);

    Z_ASSERT_N(addr_resolve("test", LSTR("127.0.0.1:1"), &su));
    sockunion_setport(&su, 0);

    server = listenx(-1, &su, 1, SOCK_STREAM, IPPROTO_TCP, 0);
    Z_ASSERT_N(server);
    zel_server_g = el_fd_register(server, true, POLLIN, &z_accept, query_cb);

    sockunion_setport(&su, getsockport(server, AF_INET));

    httpc_cfg_init(&zcfg_g);
    zcfg_g.refcnt++;
    zcfg_g.use_proxy = (flags & Z_QUERY_USE_PROXY);
    zhttpc_g = httpc_connect(&su, &zcfg_g, NULL);
    Z_ASSERT_P(zhttpc_g);

    httpc_query_init(&zquery_g);
    httpc_bufferize(&zquery_g, 40 << 20);
    zquery_g.on_hdrs = &z_query_on_hdrs;
    zquery_g.on_data = &z_query_on_data;
    zquery_g.on_done = &z_query_on_done;

    httpc_query_attach(&zquery_g, zhttpc_g);
    httpc_query_start(&zquery_g, HTTP_METHOD_GET, host, uri);
    httpc_query_hdrs_done(&zquery_g, 0, false);
    httpc_query_done(&zquery_g);

    while (!has_reply_g) {
        el_loop_timeout(10);
    }
    Z_ASSERT_EQ(zstatus_g, HTTPC_STATUS_OK);
    Z_HELPER_END;
}

static void z_query_cleanup(void) {
    httpc_query_wipe(&zquery_g);
    el_unregister(&zel_server_g);
    el_unregister(&zel_client_g);
    el_loop_timeout(10);
    sb_wipe(&body_g);
    sb_wipe(&zquery_sb_g);
}

Z_GROUP_EXPORT(httpc) {
    Z_TEST(unexpected_100_continue, "test behavior when receiving 100") {
        Z_HELPER_RUN(z_query_setup(&z_reply_100, 0,
                                   LSTR("localhost"), LSTR("/")));

        Z_ASSERT_EQ((http_code_t)HTTP_CODE_OK , code_g);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&body_g), LSTR("Coucou"));

        z_query_cleanup();
    } Z_TEST_END;

    Z_TEST(gzip_with_zero_length, "test Content-Encoding: gzip with Content-Length: 0") {
        Z_HELPER_RUN(z_query_setup(&z_reply_gzip_empty, 0,
                                   LSTR("localhost"), LSTR("/")));

        Z_ASSERT_EQ((http_code_t)HTTP_CODE_ACCEPTED , code_g);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&body_g), LSTR(""));

        z_query_cleanup();
    } Z_TEST_END;

    Z_TEST(close_with_no_content_length, "test close without Content-Length") {
        Z_HELPER_RUN(z_query_setup(&z_reply_close_without_content_length, 0,
                                   LSTR("localhost"), LSTR("/")));

        Z_ASSERT_EQ((http_code_t)HTTP_CODE_OK , code_g);
        Z_ASSERT_EQ(body_g.len, 8192 * 4096  + 4);
        Z_ASSERT_LSTREQUAL(LSTR_INIT_V(body_g.data, 4), LSTR("Plop"));
        sb_skip(&body_g, 4);
        for (int i = 0; i < body_g.len; i++) {
            Z_ASSERT_EQ(body_g.data[i], 'a' + ((i / 8192) % 26));
        }

        z_query_cleanup();
    } Z_TEST_END;

    Z_TEST(url_host_and_uri, "test hosts and URIs") {
        /* Normal usage, target separate host and URI */
        Z_HELPER_RUN(z_query_setup(&z_reply_keep, 0,
                                   LSTR("localhost"), LSTR("/coucou")));
        Z_ASSERT_EQ((http_code_t)HTTP_CODE_OK , code_g);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&body_g), LSTR("Coucou"));
        Z_ASSERT(lstr_startswith(LSTR_SB_V(&zquery_sb_g),
            LSTR("GET /coucou HTTP/1.1\r\n"
                 "Host: localhost\r\n")));
        z_query_cleanup();

        /* Proxy that target separate host and URI, URI must be transform to
         * absolute */
        Z_HELPER_RUN(z_query_setup(&z_reply_keep, Z_QUERY_USE_PROXY,
                                   LSTR("localhost"), LSTR("/coucou")));
        Z_ASSERT_EQ((http_code_t)HTTP_CODE_OK , code_g);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&body_g), LSTR("Coucou"));
        Z_ASSERT(lstr_startswith(LSTR_SB_V(&zquery_sb_g),
            LSTR("GET http://localhost/coucou HTTP/1.1\r\n"
                 "Host: localhost\r\n")));
        z_query_cleanup();

        /* same thing without leading / */
        Z_HELPER_RUN(z_query_setup(&z_reply_keep, Z_QUERY_USE_PROXY,
                                   LSTR("localhost"), LSTR("coucou")));
        Z_ASSERT_EQ((http_code_t)HTTP_CODE_OK , code_g);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&body_g), LSTR("Coucou"));
        Z_ASSERT(lstr_startswith(LSTR_SB_V(&zquery_sb_g),
            LSTR("GET http://localhost/coucou HTTP/1.1\r\n"
                 "Host: localhost\r\n")));
        z_query_cleanup();

        /* Proxy with absolute HTTP URL */
        Z_HELPER_RUN(z_query_setup(&z_reply_keep, Z_QUERY_USE_PROXY,
                                   LSTR("localhost"),
                                   LSTR("http://localhost:80/coucou")));
        Z_ASSERT_EQ((http_code_t)HTTP_CODE_OK , code_g);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&body_g), LSTR("Coucou"));
        Z_ASSERT(lstr_startswith(LSTR_SB_V(&zquery_sb_g),
            LSTR("GET http://localhost:80/coucou HTTP/1.1\r\n"
                 "Host: localhost\r\n")));
        z_query_cleanup();

        /* Same thing with HTTPS */
        Z_HELPER_RUN(z_query_setup(&z_reply_keep, Z_QUERY_USE_PROXY,
                                   LSTR("localhost"),
                                   LSTR("https://localhost:443/coucou")));
        Z_ASSERT_EQ((http_code_t)HTTP_CODE_OK , code_g);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&body_g), LSTR("Coucou"));
        Z_ASSERT(lstr_startswith(LSTR_SB_V(&zquery_sb_g),
            LSTR("GET https://localhost:443/coucou HTTP/1.1\r\n"
                 "Host: localhost\r\n")));
        z_query_cleanup();
    } Z_TEST_END;

    Z_TEST(no_content, "test a reply with NO_CONTENT code") {
        Z_HELPER_RUN(z_query_setup(&z_reply_no_content, 0,
                                   LSTR("localhost"), LSTR("/")));
        Z_ASSERT_EQ((http_code_t)HTTP_CODE_NO_CONTENT, code_g);
        z_query_cleanup();
    } Z_TEST_END;
} Z_GROUP_END;

/* }}} */
