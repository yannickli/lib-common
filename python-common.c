/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2013 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <Python.h>
#include "core.h"
#include "http.h"
#include "core.iop.h"
#include "container.h"
#include "el.h"
#include "python-common.h"

/* http {{{ */
/* type declaration {{{*/

#define PYTHON_HTTP_POOL_SIZE 4096
#define PYTHON_HTTP_MAX_PENDING 5000
#define PYTHON_HTTP_MAXRATE_DEFAULT 100
#define PYTHON_HTTP_MAXCONN_DEFAULT 10

static void python_http_on_done(httpc_query_t *q, httpc_status_t status);

static PyObject *http_initialize_error = NULL;
static PyObject *http_query_error      = NULL;

enum {
    PYTHON_HTTP_STATUS_OK = 0,
    PYTHON_HTTP_STATUS_ERROR = -1,
    PYTHON_HTTP_STATUS_USERERROR = -2,
};

/* module structure {{{*/

static struct {
    struct ev_t             *blocker;
    httpc_pool_t            *m;

    int                      nb_pending;
    dlist_t                  pending;
    mem_pool_t              *pool;

    net_rctl_t               rctl;

    core__iop_http_method__t http_method;
    lstr_t                   url;
    lstr_t                   url_args;
    int                      port;
    lstr_t                   user;
    lstr_t                   password;
    int32_t                  maxrate;

    PyObject                *cb_build_headers;
    PyObject                *cb_build_body;
    PyObject                *cb_parse_answer;
} python_http_g;
#define _G  python_http_g

PyThreadState *python_state_g;

/* }}}*/
/* python_ctx_t {{{*/

typedef struct python_ctx_t {
    PyObject     *data;
    char         *path;
    PyObject     *cb_query_done;
    dlist_t       list;
} python_ctx_t;

static python_ctx_t *python_ctx_init(python_ctx_t *ctx)
{
    p_clear(ctx, 1);
    dlist_init(&ctx->list);
    return ctx;
}
static python_ctx_t *python_ctx_new(void);
DO_MP_NEW(_G.pool, python_ctx_t, python_ctx);

static void python_ctx_wipe(python_ctx_t *ctx)
{
    p_delete(&ctx->path);
    dlist_remove(&ctx->list);
    Py_XDECREF(ctx->data);
    Py_XDECREF(ctx->cb_query_done);
}
static void python_ctx_delete(python_ctx_t **ctx);
DO_MP_DELETE(_G.pool, python_ctx_t, python_ctx);

/* }}}*/
/* python_query_t {{{*/

typedef struct python_query_t {
    python_ctx_t *ctx;
    httpc_query_t q;
} python_query_t;

static python_query_t *python_query_new(void)
{
    python_query_t *q = p_new(python_query_t, 1);

    httpc_query_init(&q->q);
    httpc_bufferize(&q->q, 1 << 20);
    q->q.on_done = python_http_on_done;
    return q;
}

static void python_query_wipe(python_query_t *q)
{
    httpc_query_wipe(&q->q);
}
GENERIC_DELETE(python_query_t, python_query);

/* }}}*/
/* }}}*/
/* private {{{*/

static void
python_http_query_end(python_ctx_t **_ctx, int status, lstr_t err_msg,
                      bool restore_Thread)
{
    python_ctx_t *ctx = *_ctx;
    PyObject     *arg = NULL;
    PyObject     *res = NULL;

    if (restore_Thread)
       PyEval_RestoreThread(python_state_g);

    arg = Py_BuildValue("Ois", ctx->data, status, err_msg.s);;
    res = PyObject_CallObject(ctx->cb_query_done, arg);
    Py_XDECREF(arg);

    Py_XDECREF(res);
    python_ctx_delete(&ctx);

    if (restore_Thread)
       python_state_g = PyEval_SaveThread();
}

static void python_http_process_answer(python_query_t *q)
{
    t_scope;

    int res = q->q.qinfo->code;

    PyObject *cbk_res = NULL;
    PyObject *arg     = NULL;
    PyObject *answer  = NULL;

    if (res != HTTP_CODE_OK) {
       python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_ERROR,
                             http_code_to_str(res), true);
       return;
    }

    PyEval_RestoreThread(python_state_g);

    answer = PyString_FromString(q->q.payload.data);
    arg = PyTuple_New(1);
    PyTuple_SetItem(arg, 0, answer);
    cbk_res = PyObject_CallObject(_G.cb_parse_answer, arg);
    Py_XDECREF(arg);

    if (!cbk_res || !PyInt_Check(cbk_res) || PyInt_AsLong(cbk_res) != 0l) {
       python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_USERERROR,
                             LSTR_NULL_V, false);
    } else {
       python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_OK,
                             LSTR_NULL_V, false);
    }

    Py_XDECREF(cbk_res);

    python_state_g = PyEval_SaveThread();
}

static void python_prepare_query(httpc_query_t *q, python_ctx_t *ctx)
{
    PyObject *data   = NULL;
    PyObject *cb_arg = NULL;
    outbuf_t *ob;

    PyEval_RestoreThread(python_state_g);

    cb_arg = PyTuple_New(1);
    PyTuple_SetItem(cb_arg, 0, ctx->data);
    Py_XINCREF(ctx->data);

    ob = httpc_get_ob(q);

    data = PyObject_CallObject(_G.cb_build_headers, cb_arg);
    if (data != NULL && PyString_Check(data) && PyString_Size(data) > 0) {
        ob_adds(ob, PyString_AsString(data));
    }
    Py_XDECREF(data);

    httpc_query_hdrs_done(q, -1, false);

    data = PyObject_CallObject(_G.cb_build_body, cb_arg);
    if (data != NULL && PyString_Check(data) && PyString_Size(data) > 0) {
        const char *body_str = PyString_AsString(data);
        ob_adds(ob, body_str);
    }
    Py_XDECREF(data);

    httpc_query_done(q);

    Py_XDECREF(cb_arg);

    python_state_g = PyEval_SaveThread();
}

static void python_http_launch_query(httpc_t *w, python_ctx_t *ctx)
{
    SB_1k(sb);
    python_query_t *q = python_query_new();

    q->ctx = ctx;
    httpc_query_attach(&q->q, w);

    if (ctx->path) {
        sb_add_urlencode(&sb, ctx->path, strlen(ctx->path));
    } else {
        sb_addc(&sb, '/');
    }

    if (_G.url_args.s)
        sb_add_lstr(&sb, _G.url_args);

    httpc_query_start_flags(&q->q, (int)_G.http_method, _G.m->host,
                            LSTR_SB_V(&sb), false);

    if (_G.user.len && _G.password.len)
        httpc_query_hdrs_add_auth(&q->q, _G.user, _G.password);

    python_prepare_query(&q->q, q->ctx);
    sb_wipe(&sb);
}

static void process_queries(httpc_pool_t *m, httpc_t *w)
{
    if (!python_state_g) {
        /*while PyEval_SaveThread is not called by python_http_loop,
         * do nothing*/
        return;
    }

    while (_G.nb_pending && net_rctl_can_fire(&_G.rctl)) {
        python_ctx_t *ctx;

        if (dlist_is_empty(&_G.pending)) {
            e_error("inconsistent nb_pending: %d", _G.nb_pending);
            _G.nb_pending = 0;
            break;
        }

        ctx = dlist_first_entry(&_G.pending, python_ctx_t, list);

        if (!w && !(w = httpc_pool_get(m))) {
            /* No connection ready */
            break;
        }

        dlist_pop(&_G.pending);
        _G.nb_pending--;
        __net_rctl_fire(&_G.rctl);
        python_http_launch_query(w, ctx);
        w = NULL;
    }
}

static void python_http_on_done(httpc_query_t *_q, httpc_status_t status)
{
    python_query_t *q = container_of(_q, python_query_t, q);


    switch(status) {
      case HTTPC_STATUS_OK:
        python_http_process_answer(q);
        break;

      case HTTPC_STATUS_ABORT:
      case HTTPC_STATUS_TIMEOUT:
        python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_ERROR,
                              LSTR_IMMED_V(
                              "HTTP request could not complete"),
                              true);
        break;

      case HTTPC_STATUS_INVALID:
      case HTTPC_STATUS_TOOLARGE:
      default:
        python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_ERROR,
                              LSTR_IMMED_V(
                              "unable to parse server response"),
                              true);
        break;
    }
    python_query_delete(&q);
    process_queries(_G.m, NULL);
}

static void python_http_on_term(el_t idx, int signum, el_data_t priv)
{
    el_blocker_unregister(&_G.blocker);
}

static void net_rtcl_on_ready(net_rctl_t *rctl)
{
    process_queries(_G.m, NULL);
}

/* }}}*/
/* public interface {{{*/

static PyObject *python_http_initialize(PyObject *self, PyObject *args)
{
    t_scope;

    PyObject   *cb_build_headers = NULL;
    PyObject   *cb_build_body    = NULL;
    PyObject   *cb_parse         = NULL;

    struct core__httpc_cfg__t iop_cfg;
    httpc_cfg_t               *cfg     = NULL;
    char                      *cfg_hex = NULL;
    lstr_t                     cfg_lstr;

    sockunion_t su;
    int         http_method;
    int         maxconn  = PYTHON_HTTP_MAXCONN_DEFAULT;
    int         maxrate  = PYTHON_HTTP_MAXRATE_DEFAULT;
    int         port;
    char       *url      = NULL;
    char       *url_arg  = NULL;
    char       *user     = NULL;
    char       *password = NULL;

    if (args == NULL
    ||  !PyArg_ParseTuple(args, "sOOOszii|iizz",
                         &cfg_hex,
                         &cb_build_headers,
                         &cb_build_body,
                         &cb_parse,
                         &url,
                         &url_arg,
                         &port,
                         &http_method,
                         &maxconn,
                         &maxrate,
                         &user,
                         &password))
    {
        PyErr_SetString(http_initialize_error,
                        "failed to parse http initialize arguments");
        return NULL;
    }

    if (!PyCallable_Check(cb_build_headers)) {
        PyErr_SetString(http_initialize_error,
                        "http initialize cb_build_headers argument is not"
                        " a callable PyObject (callback function))");
        return NULL;
    }
    if (!PyCallable_Check(cb_build_body)) {
        PyErr_SetString(http_initialize_error,
                        "http initialize build_body argument is not"
                        " a callable PyObject (callback function))");
        return NULL;
    }
    if (!PyCallable_Check(cb_parse)) {
        PyErr_SetString(http_initialize_error,
                        "http initialize parse_cbk argument is not"
                        " a callable PyObject (callback function))");
        return NULL;
    }
    if (url == NULL) {
        PyErr_SetString(http_initialize_error, "url is not defined");
        return NULL;
    }

    cfg_lstr = LSTR_STR_V(cfg_hex);
    if( t_iop_bunpack(&cfg_lstr, core, httpc_cfg, &iop_cfg) < 0) {
        PyErr_SetString(http_initialize_error, "failed to unpack http cfg");
        return NULL;
    }

    _G.url = lstr_dups(url, strlen(url));
    _G.port = port;
    _G.url_args = lstr_dups(url_arg, strlen(url_arg));

    if (addr_info_str(&su, _G.url.s, _G.port, AF_UNSPEC) < 0) {
        lstr_t str_err = lstr_fmt("unable to resolve: %s",
                                 _G.url.s);
        PyErr_SetString(http_initialize_error, str_err.s);
        return NULL;
    }

    cfg = httpc_cfg_new();
    httpc_cfg_from_iop(cfg, &iop_cfg);

    Py_XINCREF(cb_build_headers);
    Py_XINCREF(cb_build_body);
    Py_XINCREF(cb_parse);
    _G.cb_build_headers = cb_build_headers;
    _G.cb_build_body    = cb_build_body;
    _G.cb_parse_answer  = cb_parse;

    _G.user             = LSTR_STR_V(user);
    _G.password         = LSTR_STR_V(password);
    _G.http_method      = http_method;

    _G.m = httpc_pool_new();
    _G.m->su       = su;
    _G.m->on_ready = process_queries;
    _G.m->host     = lstr_fmt("%s:%d", _G.url.s, _G.port);
    _G.m->cfg      = cfg;
    _G.m->max_len  = maxconn;

    _G.pool = mem_fifo_pool_new(PYTHON_HTTP_POOL_SIZE);
    dlist_init(&_G.pending);

    net_rctl_init(&_G.rctl, maxrate, net_rtcl_on_ready);
    net_rctl_start(&_G.rctl);

    Py_RETURN_TRUE;
}

static PyObject *python_http_shutdown(PyObject *self, PyObject *arg)
{
    httpc_pool_delete(&_G.m, true);
    net_rctl_wipe(&_G.rctl);
    mem_fifo_pool_delete(&_G.pool);
    lstr_wipe(&_G.password);
    lstr_wipe(&_G.user);
    lstr_wipe(&_G.url);
    lstr_wipe(&_G.url_args);
    Py_XDECREF(_G.cb_parse_answer);
    Py_XDECREF(_G.cb_build_body);
    Py_XDECREF(_G.cb_build_headers);

    Py_RETURN_TRUE;
}

static PyObject *python_http_query(PyObject *self, PyObject *arg)
{
    PyObject     *data = NULL;
    PyObject     *cb_query_done = NULL;
    char         *path = NULL;
    python_ctx_t *ctx;

    if (!PyArg_ParseTuple(arg, "OOs",
                          &data,
                          &cb_query_done,
                          &path)) {
        PyErr_SetString(http_query_error,
                        "failed to parse http_query argument");
        return NULL;
    }
    if (!PyCallable_Check(cb_query_done)) {
        PyErr_SetString(http_query_error,
                        "http_query 2nd argument is not "
                        "a callable PytObject (callback function)");
        return NULL;
    }

    ctx = python_ctx_new();
    Py_XINCREF(data);
    ctx->data = data;
    Py_XINCREF(cb_query_done);
    ctx->cb_query_done = cb_query_done;
    ctx->path = p_strdup(path);

    if (_G.nb_pending > PYTHON_HTTP_MAX_PENDING) {
        python_http_query_end(&ctx, PYTHON_HTTP_STATUS_ERROR,
                              LSTR_IMMED_V("too many requests in queue"),
                              false);
        return NULL;
    }

    dlist_add_tail(&_G.pending, &ctx->list);
    _G.nb_pending++;

    Py_RETURN_TRUE;
}

static PyObject *loop(PyObject *self, PyObject *arg)
{
    python_state_g = PyEval_SaveThread();
    el_signal_register(SIGINT, &python_http_on_term, NULL);
    _G.blocker = el_blocker_register();
    el_loop();
    PyEval_RestoreThread(python_state_g);
    Py_RETURN_TRUE;
}

/* }}} */
/* }}} */
/* log {{{ */

static PyObject *python_log(PyObject *self, PyObject *args)
{
    Py_ssize_t  args_len;
    const char *str = NULL;
    int         log_level;

    if (args == NULL) {
       e_error("no arguments passed to python log");
       return Py_BuildValue("i", -1);
    }

    args_len = PyTuple_Size(args);
    if (args_len != 2) {
       e_error("not exactly two arguments passed to python log: %zd",
               args_len);
       return Py_BuildValue("i", -2);
    }

    if (!PyArg_ParseTuple(args, "is", &log_level, &str)) {
       e_error("failed to parse python log arguments");
       return Py_BuildValue("i", -3);
    }

    switch (log_level) {
      case LOG_CRIT:
      case LOG_ERR:
      case LOG_WARNING:
      case LOG_NOTICE:
      case LOG_INFO:
      case LOG_DEBUG:
        e_log(log_level, "%s", str);
        break;

      case 10:
        e_panic("%s", str);
        break;

      default:
        e_error("this log level is not defined: %d", log_level);
        return Py_BuildValue("i", -4);
        break;
    }

    return Py_BuildValue("i", 0);
}

/* }}} */
/* python module {{{ */

PyDoc_STRVAR(commonmodule_doc,
"The goal of this module is to bind lib-common http and log API to python.\n"
"Http: \n"
"It works asynchronously. At API initialization, 3 python callbacks are passed\n"
"as argument:\n"
"- One to build http request headers,\n"
"- a second to build http request body,\n"
"- a last to parse server answer.\n"
"The request method takes as argument a python object. When this method is\n"
"called, the API adds the request into a list. When the API processes the\n"
"request, it calls the 2 callbacks to build the request with python object as\n"
"argument and send it. When the answer is received, the API calls the last\n"
"callback with the server answer.");

static PyMethodDef myModule_methods[] = {
    {
        "log",
        python_log,
        METH_VARARGS,
        "python binding to e_log"
    },
    {
        "http_initialize",
        (PyCFunction)python_http_initialize,
        METH_VARARGS,
        "initialize http"
    },
    {
        "http_query",
        python_http_query,
        METH_VARARGS,
        "http query"
    },
    {
        "http_shutdown",
        python_http_shutdown,
        METH_VARARGS,
        "shutdown http"
    },
    {
        "http_loop",
        loop,
        METH_VARARGS,
        "loop http"
    }
};

static void
add_module_constant(PyObject *module, const char * constant_name, long value)
{
    PyObject *log_level_value = PyLong_FromLong(value);
    PyObject_SetAttrString(module, constant_name, log_level_value);
    Py_XDECREF(log_level_value);
}

qvector_t(PyMethodDef, PyMethodDef);

PyObject *python_common_initialize(const char *name, PyMethodDef methods[])
{
    PyObject *common_module;
    qv_t(PyMethodDef) vec;
    int pos = 0;

    qv_init(PyMethodDef, &vec);
    p_copy(qv_growlen(PyMethodDef, &vec, countof(myModule_methods)),
           myModule_methods, countof(myModule_methods));

    do {
        qv_append(PyMethodDef, &vec, methods[pos]);
    } while (methods[pos++].ml_name != NULL);

    common_module = Py_InitModule3(name, vec.tab, commonmodule_doc);

    http_initialize_error = PyErr_NewException((char *)"http.initializeError",
                                               NULL, NULL);
    Py_XINCREF(http_initialize_error);
    PyModule_AddObject(common_module, "initializeError",
                       http_initialize_error);
    http_query_error = PyErr_NewException((char *) "http.queryError",
                                          NULL, NULL);
    Py_XINCREF(http_query_error);
    PyModule_AddObject(common_module, "queryError", http_query_error);

    add_module_constant(common_module, "LOG_CRIT", LOG_CRIT);
    add_module_constant(common_module, "LOG_ERR", LOG_ERR);
    add_module_constant(common_module, "LOG_WARNING", LOG_WARNING);
    add_module_constant(common_module, "LOG_NOTICE", LOG_NOTICE);
    add_module_constant(common_module, "LOG_INFO", LOG_INFO);
    add_module_constant(common_module, "LOG_DEBUG", LOG_DEBUG);
    add_module_constant(common_module, "LOG_PANIC", 10l);

    add_module_constant(common_module, "HTTP_OK",
                        PYTHON_HTTP_STATUS_OK);
    add_module_constant(common_module, "HTTP_ERROR",
                        PYTHON_HTTP_STATUS_ERROR);
    add_module_constant(common_module, "HTTP_USERERROR",
                        PYTHON_HTTP_STATUS_USERERROR);

    return common_module;
}

/* }}} */

