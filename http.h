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

#ifndef IS_LIB_INET_HTTP_H
#define IS_LIB_INET_HTTP_H

#include "el.h"
#include "net.h"
#include "str-outbuf.h"

typedef enum http_method_t {
    HTTP_METHOD_ERROR = -1,
    /* rfc 2616: §5.1.1: Method */
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_GET,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_TRACE,
    HTTP_METHOD_CONNECT,
    HTTP_METHOD__MAX,
} http_method_t;
extern char const * const http_method_str[HTTP_METHOD__MAX];

typedef enum http_code_t {
    HTTP_CODE_CONTINUE                 = 100,
    HTTP_CODE_SWITCHING_PROTOCOL       = 101,

    HTTP_CODE_OK                       = 200,
    HTTP_CODE_CREATED                  = 201,
    HTTP_CODE_ACCEPTED                 = 202,
    HTTP_CODE_NON_AUTHORITATIVE        = 203,
    HTTP_CODE_NO_CONTENT               = 204,
    HTTP_CODE_RESET_CONTENT            = 205,
    HTTP_CODE_PARTIAL_CONTENT          = 206,

    HTTP_CODE_MULTIPLE_CHOICES         = 300,
    HTTP_CODE_MOVED_PERMANENTLY        = 301,
    HTTP_CODE_FOUND                    = 302,
    HTTP_CODE_SEE_OTHER                = 303,
    HTTP_CODE_NOT_MODIFIED             = 304,
    HTTP_CODE_USE_PROXY                = 305,
    HTTP_CODE_TEMPORARY_REDIRECT       = 307,

    HTTP_CODE_BAD_REQUEST              = 400,
    HTTP_CODE_UNAUTHORIZED             = 401,
    HTTP_CODE_PAYMENT_REQUIRED         = 402,
    HTTP_CODE_FORBIDDEN                = 403,
    HTTP_CODE_NOT_FOUND                = 404,
    HTTP_CODE_METHOD_NOT_ALLOWED       = 405,
    HTTP_CODE_NOT_ACCEPTABLE           = 406,
    HTTP_CODE_PROXY_AUTH_REQUIRED      = 407,
    HTTP_CODE_REQUEST_TIMEOUT          = 408,
    HTTP_CODE_CONFLICT                 = 409,
    HTTP_CODE_GONE                     = 410,
    HTTP_CODE_LENGTH_REQUIRED          = 411,
    HTTP_CODE_PRECONDITION_FAILED      = 412,
    HTTP_CODE_REQUEST_ENTITY_TOO_LARGE = 413,
    HTTP_CODE_REQUEST_URI_TOO_LARGE    = 414,
    HTTP_CODE_UNSUPPORTED_MEDIA_TYPE   = 415,
    HTTP_CODE_REQUEST_RANGE_UNSAT      = 416,
    HTTP_CODE_EXPECTATION_FAILED       = 417,

    HTTP_CODE_INTERNAL_SERVER_ERROR    = 500,
    HTTP_CODE_NOT_IMPLEMENTED          = 501,
    HTTP_CODE_BAD_GATEWAY              = 502,
    HTTP_CODE_SERVICE_UNAVAILABLE      = 503,
    HTTP_CODE_GATEWAY_TIMEOUT          = 504,
    HTTP_CODE_VERSION_NOT_SUPPORTED    = 505,
} http_code_t;
const char *http_code_to_str(http_code_t code);

typedef enum http_wkhdr_t {
    HTTP_WKHDR_OTHER_HEADER = -1,

    /* rfc 2616: §4.5: General Header Fields */
#define HTTP_WKHDR__GENERAL_FIRST  HTTP_WKHDR_CACHE_CONTROL
    HTTP_WKHDR_CACHE_CONTROL,
    HTTP_WKHDR_CONNECTION,
    HTTP_WKHDR_DATE,
    HTTP_WKHDR_PRAGMA,
    HTTP_WKHDR_TRAILER,
    HTTP_WKHDR_TRANSFER_ENCODING,
    HTTP_WKHDR_UPGRADE,
    HTTP_WKHDR_VIA,
    HTTP_WKHDR_WARNING,
#define HTTP_WKHDR__GENERAL_LAST   HTTP_WKHDR_WARNING

#define HTTP_WKHDR__REQRES_FIRST   HTTP_WKHDR_ACCEPT
    /* rfc 2616: §5.3: Request Header Fields */
    HTTP_WKHDR_ACCEPT,
    HTTP_WKHDR_ACCEPT_CHARSET,
    HTTP_WKHDR_ACCEPT_ENCODING,
    HTTP_WKHDR_ACCEPT_LANGUAGE,
    HTTP_WKHDR_AUTHORIZATION,
    HTTP_WKHDR_EXPECT,
    HTTP_WKHDR_FROM,
    HTTP_WKHDR_HOST,
    HTTP_WKHDR_IF_MATCH,
    HTTP_WKHDR_IF_MODIFIED_SINCE,
    HTTP_WKHDR_IF_NONE_MATCH,
    HTTP_WKHDR_IF_RANGE,
    HTTP_WKHDR_IF_UNMODIFIED_SINCE,
    HTTP_WKHDR_MAX_FORMWARDS,
    HTTP_WKHDR_PROXY_AUTHORIZATION,
    HTTP_WKHDR_RANGE,
    HTTP_WKHDR_REFERER,
    HTTP_WKHDR_TE,
    HTTP_WKHDR_USER_AGENT,

    /* rfc 2616: §6.2: Response header Fields */
    HTTP_WKHDR_ACCEPT_RANGES,
    HTTP_WKHDR_AGE,
    HTTP_WKHDR_ETAG,
    HTTP_WKHDR_LOCATION,
    HTTP_WKHDR_PROXY_AUTHENTICATE,
    HTTP_WKHDR_RETRY_AFTER,
    HTTP_WKHDR_SERVER,
    HTTP_WKHDR_VARY,
    HTTP_WKHDR_WWW_AUTHENTICATE,
#define HTTP_WKHDR__REQRES_LAST    HTTP_WKHDR_WWW_AUTHENTICATE

    /* rfc 2616: §7.1: Entity Header Fields */
#define HTTP_WKHDR__ENTITY_FIRST   HTTP_WKHDR_ALLOW
    HTTP_WKHDR_ALLOW,
    HTTP_WKHDR_CONTENT_ENCODING,
    HTTP_WKHDR_CONTENT_LANGUAGE,
    HTTP_WKHDR_CONTENT_LENGTH,
    HTTP_WKHDR_CONTENT_LOCATION,
    HTTP_WKHDR_CONTENT_MD5,
    HTTP_WKHDR_CONTENT_RANGE,
    HTTP_WKHDR_CONTENT_TYPE,
    HTTP_WKHDR_EXPIRES,
    HTTP_WKHDR_LAST_MODIFIED,
#define HTTP_WKHDR__ENTITY_LAST    HTTP_WKHDR_LAST_MODIFIED

    /* Useful headers */
    HTTP_WKHDR_SOAPACTION,

    HTTP_WKHDR__MAX,
} http_wkhdr_t;
extern char const * const http_whdr_str[HTTP_WKHDR__MAX];
http_wkhdr_t http_wkhdr_from_ps(pstream_t ps);

extern struct http_params {
    unsigned header_line_max;        /**< Maximum line length in headers */
    unsigned header_size_max;        /**< Maximum total size for headers */
    unsigned outbuf_max_size;
    unsigned on_data_threshold;
    uint16_t pipeline_depth_in;
    uint16_t pipeline_depth_out;
    unsigned noact_delay;
    unsigned max_queries_in;
    unsigned max_queries_out;
    unsigned max_conns_in;
} http_params_g;

#define HTTP_MK_VERSION(M, m)  (((M) << 8) | (m))
#define HTTP_1_0               HTTP_MK_VERSION(1, 0)
#define HTTP_1_1               HTTP_MK_VERSION(1, 1)
#define HTTP_MINOR(v)          ((v) & 0xf)
#define HTTP_MAJOR(v)          ((uint16_t)(v) >> 8)

typedef struct http_qhdr_t {
    int       wkhdr;
    pstream_t key;
    pstream_t val;
} http_qhdr_t;

enum http_parser_state {
    HTTP_PARSER_IDLE,
    HTTP_PARSER_BODY,
    HTTP_PARSER_CHUNK_HDR,
    HTTP_PARSER_CHUNK,
    HTTP_PARSER_CHUNK_TRAILER,
    HTTP_PARSER_CLOSE,
};

/**************************************************************************/
/* HTTP Server                                                            */
/**************************************************************************/

struct httpd_query_t;
typedef struct httpd_qinfo_t httpd_qinfo_t;

typedef struct httpd_cfg_t httpd_cfg_t;
typedef struct httpd_trigger_t    httpd_trigger_t;
typedef struct httpd_trigger_cb_t httpd_trigger_cb_t;

qm_kvec_t(http_path, clstr_t, httpd_trigger_t *,
          qhash_clstr_hash, qhash_clstr_equal);

typedef struct httpd_t {
    httpd_cfg_t       *cfg;
    el_t               ev;
    sb_t               ibuf;

    flag_t             connection_close   : 1;
    uint8_t            state;
    uint16_t           queries;
    uint16_t           queries_done;
    unsigned           max_queries;
    unsigned           chunk_length;

    dlist_t            query_list;
    outbuf_t           ob;
} httpd_t;

typedef void (httpd_trigger_auth_f)(httpd_trigger_cb_t *, struct httpd_query_t *,
                                    pstream_t user, pstream_t pw);

struct httpd_trigger_cb_t {
    char  *auth_realm;
    httpd_trigger_auth_f *auth;
    const object_class_t *query_cls;
    void (*cb)(httpd_trigger_cb_t *, struct httpd_query_t *, const httpd_qinfo_t *);
    void (*destroy)(httpd_trigger_cb_t *);
    void (*on_query_wipe)(struct httpd_query_t *q);
};

struct httpd_trigger_t {
    qm_t(http_path)       childs;
    httpd_trigger_cb_t   *cb;
    char                  path[];
};

struct httpd_cfg_t {
    int      refcnt;
    unsigned nb_conns;

    unsigned outbuf_max_size;
    unsigned on_data_threshold;
    unsigned max_queries;
    unsigned noact_delay;
    unsigned max_conns;
    uint16_t pipeline_depth;

    httpd_trigger_t roots[HTTP_METHOD_DELETE + 1];
};
httpd_cfg_t *httpd_cfg_init(httpd_cfg_t *cfg);
void         httpd_cfg_wipe(httpd_cfg_t *cfg);
DO_REFCNT(httpd_cfg_t, httpd_cfg);

el_t     httpd_listen(sockunion_t *su, httpd_cfg_t *);
void     httpd_unlisten(el_t *ev);
httpd_t *httpd_spawn(int fd, httpd_cfg_t *);

GENERIC_INIT(httpd_trigger_cb_t, httpd_trigger_cb);
GENERIC_NEW(httpd_trigger_cb_t, httpd_trigger_cb);
void httpd_trigger_cb_destroy(httpd_trigger_cb_t *cb);

httpd_trigger_cb_t *
httpd_trigger_register_(httpd_trigger_t *, const char *path,
                        httpd_trigger_cb_t *cb);
httpd_trigger_cb_t *
httpd_trigger_unregister_(httpd_trigger_t *, const char *path);

static inline void
httpd_trigger_cb__set_auth(httpd_trigger_cb_t *cb, httpd_trigger_auth_f *auth,
                           const char *auth_realm)
{
    p_delete(&cb->auth_realm);
    cb->auth_realm = p_strdup(auth_realm ?: "Intersec HTTP Server");
    cb->auth       = auth;
}

#define httpd_trigger_register(cfg, m, p, cb) \
    httpd_trigger_register_(&(cfg)->roots[HTTP_METHOD_##m], p, cb)
#define httpd_trigger_unregister(cfg, m, p) \
    httpd_trigger_unregister_(&(cfg)->roots[HTTP_METHOD_##m], p)


/**************************************************************************/
/* HTTP Server Queries related                                            */
/**************************************************************************/

struct httpd_qinfo_t {
    http_method_t method;
    uint16_t      http_version;
    uint16_t      hdrs_len;

    pstream_t     host;
    pstream_t     prefix;
    pstream_t     query;
    pstream_t     vars;

    pstream_t     hdrs_ps;
    http_qhdr_t  *hdrs;
};

/** \typedef http_query_t.
 * \brief HTTP Query base class.
 *
 * An http_query_t is the base class for queries received on an #httpd_t.
 *
 * It is refcounted, and is valid until it's answered (no matter if the
 * underlying #httpd_t available as http_query_t#owner is still valid or not).
 *
 * As a consequence it means that nobody should ever suppose that a query is
 * still valid once #httpd_reply_done() #httpd_reject() or any function using
 * them internaly (#httpd_reply_202accepted() e.g.) is called.
 *
 * If this is required, then use #obj_retain() and #obj_release() accordingly
 * to ensure the liveness of the #httpd_query_t.
 *
 * <h1>How to use an #http_query_t</h1>
 *
 *   When a the Headers of an HTTP query is received, the matching
 *   #httpd_trigger_t is looked up, the #httpd_query_t (or a subclass if
 *   httpd_trigger_t#query_cls is set) is created. Then if there is a
 *   httpd_trigger_t#auth callback, it is called (possibly with empty
 *   #pstream_t's if there is no Authentication header).
 *
 *   If the authentication callback hasn't rejected the query, then the
 *   httpd_trigger_t#cb callback is called with the created query (no body
 *   still has been received at this point). This is the moment to setup the
 *   #httpd_query_t on_data/on_done/on_ready e.g. using #httpd_bufferize().
 *
 *   Note that the httpd_query_t#on_done hook may never ever be called if the
 *   HTTP query was invalid or the connection lost. That's why it's important
 *   to properly use the httpd_trigger_t#on_query_wipe hook (for when no
 *   subclassing of the #httpd_query_t has been done and its
 *   httpd_query_t#priv pointer is used) or properly augment the
 *   httpd_query_t#wipe implementation to wipe all memory clean.
 *
 * <h1>Important considerations</h1>
 *
 *   #httpd_query_t can be answered to asynchronously, but that does not mean
 *   that the underlying connection is still here. One can know at each time
 *   if there is still a connection looking at httpd_query_t#owner pointer. If
 *   it's NULL, the #httpd_t is dead. In that case it is correct to
 *   #obj_release() the query and go away since anything that would else be
 *   answered would be discarded anyway.
 */
#define HTTPD_QUERY_FIELDS(pfx) \
    OBJECT_FIELDS(pfx);                                             \
                                                                    \
    httpd_t            *owner;                                      \
    httpd_trigger_cb_t *trig_cb;                                    \
    dlist_t             query_link;                                 \
                                                                    \
    int16_t             refcnt;                                     \
                                                                    \
    /* User flags    */                                             \
    flag_t              traced        : 1;                          \
                                                                    \
    /* Input related */                                             \
    flag_t              expect100cont : 1;                          \
    flag_t              parsed        : 1;                          \
                                                                    \
    /* Output related */                                            \
    flag_t              own_ob        : 1;                          \
    flag_t              hdrs_started  : 1;                          \
    flag_t              hdrs_done     : 1;                          \
    flag_t              chunk_started : 1;                          \
    flag_t              answered      : 1;                          \
    flag_t              chunked       : 1;                          \
    flag_t              conn_close    : 1;                          \
                                                                    \
    uint16_t            answer_code;                                \
    uint16_t            http_version;                               \
    time_t              query_sec;                                  \
    unsigned            query_usec;                                 \
                                                                    \
    int                 chunk_hdr_offs;                             \
    int                 chunk_prev_length;                          \
    unsigned            payload_max_size;                           \
                                                                    \
    sb_t                payload;                                    \
    outbuf_t           *ob;                                         \
    httpd_qinfo_t      *qinfo;                                      \
    void               *priv;                                       \
                                                                    \
    void              (*on_data)(httpd_query_t *q, pstream_t ps);   \
    void              (*on_done)(httpd_query_t *q)

#define HTTPD_QUERY_METHODS(type_t) \
    OBJECT_METHODS(type_t)

OBJ_CLASS(httpd_query, object, HTTPD_QUERY_FIELDS, HTTPD_QUERY_METHODS);

#define httpd_query_dup(q)  ({ typeof(*(q)) *__q = (q); __q->refcnt++; q; })

void httpd_bufferize(httpd_query_t *q, unsigned maxsize);

/*---- headers utils ----*/

httpd_qinfo_t *httpd_qinfo_dup(const httpd_qinfo_t *info);
static inline void httpd_qinfo_delete(httpd_qinfo_t **infop) {
    p_delete(infop);
}
int t_httpd_qinfo_get_basic_auth(const httpd_qinfo_t *info,
                                 pstream_t *user, pstream_t *pw);

/*---- low level httpd_query reply functions ----*/

static inline outbuf_t *httpd_get_ob(httpd_query_t *q)
{
    if (unlikely(!q->ob)) {
        q->own_ob = true;
        q->ob     = ob_new();
    }
    return q->ob;
}

outbuf_t *httpd_reply_hdrs_start(httpd_query_t *q, int code, bool cacheable);
void      httpd_put_date_hdr(outbuf_t *ob, const char *hdr, time_t now);
void      httpd_reply_hdrs_done(httpd_query_t *q, int content_length, bool chunked);
void      httpd_reply_done(httpd_query_t *q);

static inline void httpd_reply_chunk_start(httpd_query_t *q, outbuf_t *ob)
{
    if (!q->chunked)
        return;
    assert (!q->chunk_started);
    q->chunk_started     = true;
    q->chunk_hdr_offs    = ob_reserve(ob, 12);
    q->chunk_prev_length = ob->length;
}

void httpd_reply_chunk_done_(httpd_query_t *q, outbuf_t *ob);
static inline void httpd_reply_chunk_done(httpd_query_t *q, outbuf_t *ob)
{
    if (q->chunked)
        httpd_reply_chunk_done_(q, ob);
}

/*---- high level httpd_query reply functions ----*/

void httpd_reply_100continue(httpd_query_t *q);
void httpd_reply_202accepted(httpd_query_t *q);

__attribute__((format(printf, 3, 4)))
void httpd_reject_(httpd_query_t *q, int code, const char *fmt, ...);
#define httpd_reject(q, code, fmt, ...) \
    httpd_reject_(q, HTTP_CODE_##code, fmt, ##__VA_ARGS__)
void httpd_reject_unauthorized(httpd_query_t *q, const char *auth_realm);


/*---- http-srv-static.c ----*/
void httpd_reply_make_index(httpd_query_t *q, int dirfd, bool head);
void httpd_reply_file(httpd_query_t *q, int dirfd, const char *file, bool head);

httpd_trigger_cb_t *httpd_trigger__static_dir(const char *path);


/**************************************************************************/
/* HTTP Client                                                            */
/**************************************************************************/

typedef struct httpc_pool_t httpc_pool_t;

typedef struct httpc_cfg_t {
    int          refcnt;

    flag_t       use_proxy : 1;
    uint16_t     pipeline_depth;
    unsigned     noact_delay;
    unsigned     max_queries;
    unsigned     on_data_threshold;
} httpc_cfg_t;
httpc_cfg_t *httpc_cfg_init(httpc_cfg_t *cfg);
void         httpc_cfg_wipe(httpc_cfg_t *cfg);
DO_REFCNT(httpc_cfg_t, httpc_cfg);

typedef struct httpc_t {
    httpc_pool_t *pool;
    httpc_cfg_t  *cfg;
    dlist_t       pool_link;
    el_t          ev;
    sb_t          ibuf;

    flag_t        connection_close : 1;
    flag_t        busy             : 1;
    uint8_t       state;
    uint16_t      queries;
    unsigned      chunk_length;
    unsigned      max_queries;

    dlist_t       query_list;
    outbuf_t      ob;
} httpc_t;

httpc_t *httpc_spawn(int fd, httpc_cfg_t *, httpc_pool_t *);
httpc_t *httpc_connect(const sockunion_t *, httpc_cfg_t *, httpc_pool_t *);
void     httpc_close(httpc_t **);

struct httpc_pool_t {
    httpc_cfg_t *cfg;
    char        *host;
    sockunion_t  su;

    int          len;
    int          max_len;
    int         *len_global;
    int          max_len_global;
    dlist_t      ready_list;
    dlist_t      busy_list;

    void       (*on_ready)(httpc_pool_t *, httpc_t *);
    void       (*on_busy)(httpc_pool_t *, httpc_t *);
};

httpc_pool_t *httpc_pool_init(httpc_pool_t *);
void httpc_pool_close_clients(httpc_pool_t *);
void httpc_pool_wipe(httpc_pool_t *);
GENERIC_NEW(httpc_pool_t, httpc_pool);
GENERIC_DELETE(httpc_pool_t, httpc_pool);

void httpc_pool_detach(httpc_t *w);
void httpc_pool_attach(httpc_t *w, httpc_pool_t *pool);
httpc_t *httpc_pool_launch(httpc_pool_t *pool);
httpc_t *httpc_pool_get(httpc_pool_t *pool);

static inline void httpc_set_ready(httpc_t *w)
{
    httpc_pool_t *pool = w->pool;

    if (unlikely(!w->busy))
        return;
    w->busy = false;
    if (pool) {
        dlist_move(&pool->ready_list, &w->pool_link);
        if (pool->on_ready)
            (*pool->on_ready)(pool, w);
    }
}

static inline void httpc_set_busy(httpc_t *w)
{
    httpc_pool_t *pool = w->pool;

    if (unlikely(w->busy))
        return;
    w->busy = true;
    if (pool) {
        dlist_move(&pool->busy_list, &w->pool_link);
        if (pool->on_busy)
            (*pool->on_busy)(pool, w);
    }
}

/**************************************************************************/
/* HTTP Client Queries                                                    */
/**************************************************************************/

typedef enum httpc_status_t {
    HTTPC_STATUS_OK,
    HTTPC_STATUS_INVALID    = -1,
    HTTPC_STATUS_ABORT      = -2,
    HTTPC_STATUS_TOOLARGE   = -3,
    HTTPC_STATUS_EXP100CONT = -4,
} httpc_status_t;

typedef struct httpc_qinfo_t {
    http_code_t  code;
    uint16_t     http_version;
    uint16_t     hdrs_len;

    pstream_t    reason;
    pstream_t    hdrs_ps;
    http_qhdr_t *hdrs;
} httpc_qinfo_t;

typedef struct httpc_query_t httpc_query_t;
struct httpc_query_t {
    httpc_t       *owner;
    dlist_t        query_link;
    httpc_qinfo_t *qinfo;
    sb_t           payload;
    unsigned       payload_max_size;

    int            chunk_hdr_offs;
    int            chunk_prev_length;
    flag_t         hdrs_started  : 1;
    flag_t         hdrs_done     : 1;
    flag_t         chunked       : 1;
    flag_t         chunk_started : 1;
    flag_t         query_done    : 1;
    flag_t         expect100cont : 1;

    void         (*on_100cont)(httpc_query_t *q);
    int          (*on_hdrs)(httpc_query_t *q);
    int          (*on_data)(httpc_query_t *q, pstream_t ps);
    void         (*on_done)(httpc_query_t *q, httpc_status_t status);
};
void httpc_query_init(httpc_query_t *q);
void httpc_query_reset(httpc_query_t *q);
void httpc_query_wipe(httpc_query_t *q);
void httpc_query_attach(httpc_query_t *q, httpc_t *w);

void httpc_bufferize(httpc_query_t *q, unsigned maxsize);

static ALWAYS_INLINE outbuf_t *httpc_get_ob(httpc_query_t *q) {
    return &q->owner->ob;
}

void httpc_query_start(httpc_query_t *q, http_method_t m,
                       const char *host, const char *uri);
void httpc_query_hdrs_done(httpc_query_t *q, int clen, bool chunked);
void httpc_query_done(httpc_query_t *q);

static inline void httpc_query_chunk_start(httpc_query_t *q, outbuf_t *ob)
{
    if (!q->chunked)
        return;
    assert (!q->chunk_started);
    q->chunk_started     = true;
    q->chunk_hdr_offs    = ob_reserve(ob, 12);
    q->chunk_prev_length = ob->length;
}

void httpc_query_chunk_done_(httpc_query_t *q, outbuf_t *ob);
static inline void httpc_query_chunk_done(httpc_query_t *q, outbuf_t *ob)
{
    if (q->chunked)
        httpc_query_chunk_done_(q, ob);
}

#endif
