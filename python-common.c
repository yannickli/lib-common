/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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
#include <structmember.h>
#include "core.h"
#include "http.h"
#include "el.h"
#include "datetime.h"
#include "log.h"
#include "python-common.h"
#include "core.iop.h"

typedef struct pylogger_object_t pylogger_object_t;
qh_khptr_t(pylogger, pylogger_object_t);

static struct {
    struct ev_t             *blocker;
    httpc_pool_t            *m;

    int                      nb_pending;
    dlist_t                  pending;
    mem_pool_t              *pool;
    bool                     connection_ready : 1;
    bool                     first_connection : 1;

    net_rctl_t               rctl;

    core__iop_http_method__t http_method;
    lstr_t                   url;
    lstr_t                   url_args;
    int                      port;
    lstr_t                   user;
    lstr_t                   password;
    uint32_t                 queue_timeout;

    PyObject                *cb_build_headers;
    PyObject                *cb_build_body;
    PyObject                *cb_parse_answer;

    qh_t(pylogger)           pyloggers;
    bool                     pylogger_class_initialized : 1;

    bool                     py_el_module_registered : 1;
    bool                     py_el_types_registered : 1;
    python_el_cfg_t          py_el_cfg;
    qh_t(ptr)                py_els;
} python_http_g;
#define _G  python_http_g

PyThreadState *python_state_g;

/* {{{ Helper */

#define PYTHON_EXN_DEFAULT_MSG "failed to fetch exception traceback"

static PyObject *py_bytes_from_obj(PyObject *o)
{
    if (Py_TYPE(o) == &PyUnicode_Type) {
        return PyUnicode_AsEncodedString(o, "utf-8", "strict");
    }
    return PyObject_Str(o);
}

int sb_add_py_obj(sb_t *sb, PyObject *o)
{
    PyObject *obj;
    char *tmp;
    ssize_t len;

    obj = RETHROW_PN(py_bytes_from_obj(o));

    if (PyBytes_AsStringAndSize(obj, &tmp, &len) >= 0) {
        sb_add(sb, tmp, len);
    }
    Py_DECREF(obj);

    return 0;
}

void sb_add_py_traceback(sb_t *err)
{
    PyObject *type;
    PyObject *value;
    PyObject *traceback;
    PyObject *tc_str;
    PyObject *tb_module = NULL;

    PyErr_Fetch(&type, &value, &traceback);
    if (unlikely(!type || !value || !traceback
    || !(tb_module = PyImport_ImportModule("traceback"))))
    {
        if (!value || sb_add_py_obj(err, value) < 0) {
            sb_adds(err, PYTHON_EXN_DEFAULT_MSG);
        }
        goto wipe;
    }

    tc_str = PyObject_CallMethod(tb_module, (char *)"format_exception",
                                 (char *)"OOO", type, value, traceback);
    if (unlikely(!tc_str)) {
        sb_adds(err, PYTHON_EXN_DEFAULT_MSG);
    } else {
        PyObject *string;
        PyObject *ret;

        string = PyUnicode_FromString("\n");
        ret = PyUnicode_Join(string, tc_str);
        if (sb_add_py_obj(err, ret) < 0) {
            sb_adds(err, PYTHON_EXN_DEFAULT_MSG);
        }

        Py_DECREF(string);
        Py_DECREF(tc_str);
        Py_DECREF(ret);
    }

  wipe:
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    Py_XDECREF(tb_module);
}

#undef PYTHON_EXN_DEFAULT_MSG

/* }}} */
/* http {{{ */
/* type declaration {{{*/

#define PYTHON_HTTP_POOL_SIZE 4096
#define PYTHON_HTTP_MAX_PENDING 5000
#define PYTHON_HTTP_MAXRATE_DEFAULT 100
#define PYTHON_HTTP_MAXCONN_DEFAULT 10

static void python_http_on_done(httpc_query_t *q, httpc_status_t status);

static PyObject *http_initialize_error = NULL;
static PyObject *http_query_error      = NULL;
static PyObject *http_build_part_error = NULL;

enum {
    PYTHON_HTTP_STATUS_OK = 0,
    PYTHON_HTTP_STATUS_ERROR = -1,
    PYTHON_HTTP_STATUS_USERERROR = -2,
    PYTHON_HTTP_STATUS_BUILD_QUERY_ERROR = -3,
};

/* {{{ python_ctx_t */

typedef struct python_ctx_t {
    PyObject *data;
    lstr_t    path;
    lstr_t    url_args;
    PyObject *cb_query_done;
    dlist_t   list;
    time_t    expiry;
    core__iop_http_method__t http_method;
    bool encode_url;
} python_ctx_t;

static python_ctx_t *python_ctx_init(python_ctx_t *ctx)
{
    p_clear(ctx, 1);
    dlist_init(&ctx->list);
    ctx->expiry = _G.queue_timeout > 0 ? lp_getsec() + _G.queue_timeout : 0;
    return ctx;
}

static python_ctx_t *python_ctx_new(void);
DO_MP_NEW(_G.pool, python_ctx_t, python_ctx);

static void python_ctx_wipe(python_ctx_t *ctx)
{
    lstr_wipe(&ctx->path);
    lstr_wipe(&ctx->url_args);
    dlist_remove(&ctx->list);
    Py_XDECREF(ctx->data);
    Py_XDECREF(ctx->cb_query_done);
}

static void python_ctx_delete(python_ctx_t **ctx);
DO_MP_DELETE(_G.pool, python_ctx_t, python_ctx);

/* }}}*/
/* {{{ python_query_t */

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
/* {{{ private */

static void
python_http_query_end(python_ctx_t **_ctx, int status, lstr_t err_msg,
                      bool restore_Thread)
{
    python_ctx_t *ctx = *_ctx;
    PyObject     *res = NULL;
    SB_1k(err);

    if (restore_Thread) {
        PyEval_RestoreThread(python_state_g);
    }

    res = PY_TRY_CATCH(PyObject_CallFunction(ctx->cb_query_done,
                                             (char *)"Ois", ctx->data, status,
                                             err_msg.s),
                       &err);
    if (!res) {
        e_error("query_done callback raised an exception: \n%*pM",
                SB_FMT_ARG(&err));
    } else {
        Py_DECREF(res);
    }

    python_ctx_delete(&ctx);

    if (restore_Thread) {
        python_state_g = PyEval_SaveThread();
    }
}

static void python_http_process_answer(python_query_t *q)
{
    PyObject *cbk_res = NULL;
    SB_8k(err);

    PyEval_RestoreThread(python_state_g);

    cbk_res = PY_TRY_CATCH(PyObject_CallFunction(_G.cb_parse_answer,
                                                 (char *)"z#Oi",
                                                 q->q.payload.data,
                                                 q->q.payload.len,
                                                 q->ctx->data,
                                                 q->q.qinfo->code),
                           &err);
    if (!cbk_res) {
        lstr_t err_msg = LSTR_IMMED_V("parse callback raised an exception");

        e_error("%*pM: \n%*pM", LSTR_FMT_ARG(err_msg), SB_FMT_ARG(&err));
        python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_USERERROR,
                              err_msg, false);
        goto end;
    }

    if (!PyInt_Check(cbk_res) || PyInt_AsLong(cbk_res) != 0l) {
        lstr_t err_msg = LSTR_IMMED_V("parse callback does not return 0");

        python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_USERERROR,
                              err_msg, false);
    } else {
        python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_OK,
                              LSTR_NULL_V, false);
    }

    Py_DECREF(cbk_res);

  end:
    python_state_g = PyEval_SaveThread();
}

static PyObject *build_part(python_ctx_t *ctx, PyObject *cb,
                            PyObject *cb_args, const char *cb_name)
{
    t_scope;
    PyObject *res;
    SB_8k(err);
    lstr_t err_msg;

    if ((res = PY_TRY_CATCH(PyObject_CallObject(cb, cb_args), &err))) {
        return res;
    }

    err_msg = t_lstr_fmt("%s callback raised an exception", cb_name);
    e_error("%*pM: \n%*pM", LSTR_FMT_ARG(err_msg), SB_FMT_ARG(&err));

    python_http_query_end(&ctx, PYTHON_HTTP_STATUS_USERERROR, err_msg, false);

    return NULL;
}

static void python_http_launch_query(httpc_t *w, python_ctx_t *ctx)
{
    PyObject *hdr  = NULL;
    PyObject *body = NULL;
    PyObject *cb_arg;
    python_query_t *q;
    outbuf_t *ob;
    SB_1k(sb);

    /* build headers and body data */
    PyEval_RestoreThread(python_state_g);

    cb_arg = PyTuple_New(1);
    PyTuple_SetItem(cb_arg, 0, ctx->data);
    Py_XINCREF(ctx->data);

    if (!(hdr = build_part(ctx, _G.cb_build_headers, cb_arg, "build_headers"))
    ||  !(body = build_part(ctx, _G.cb_build_body, cb_arg, "build_body")))
    {
        goto wipe;
    }

    /* launch query */
    q = python_query_new();
    q->ctx = ctx;

    httpc_query_attach(&q->q, w);

    if (ctx->path.s) {
        if (ctx->encode_url) {
            sb_add_urlencode(&sb, ctx->path.s, ctx->path.len);
        } else {
            sb_add_lstr(&sb, ctx->path);
        }
    } else {
        sb_addc(&sb, '/');
    }

    if (ctx->url_args.s) {
        sb_add_lstr(&sb, ctx->url_args);
    } else
    if (_G.url_args.s) {
        sb_add_lstr(&sb, _G.url_args);
    }

    httpc_query_start_flags(&q->q, (int)ctx->http_method, _G.m->host,
                            LSTR_SB_V(&sb), false);

    if (_G.user.len && _G.password.len) {
        httpc_query_hdrs_add_auth(&q->q, _G.user, _G.password);
    }

    ob = httpc_get_ob(&q->q);
    if (PyString_Check(hdr) && PyString_Size(hdr) > 0) {
        ob_adds(ob, PyString_AsString(hdr));
    }
    httpc_query_hdrs_done(&q->q, -1, false);

    if (PyString_Check(body) && PyString_Size(body) > 0) {
        ob_adds(ob, PyString_AsString(body));
    }

    httpc_query_done(&q->q);

  wipe:
    Py_DECREF(cb_arg);
    Py_XDECREF(hdr);
    Py_XDECREF(body);
    python_state_g = PyEval_SaveThread();

}

static void process_queries(httpc_pool_t *m, httpc_t *w)
{
    if (!python_state_g) {
        /* while PyEval_SaveThread is not called by python_http_loop,
         * do nothing.
         */
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

        if (ctx->expiry && ctx->expiry < lp_getsec()) {
            dlist_pop(&_G.pending);
            _G.nb_pending--;
            python_http_query_end(&ctx, PYTHON_HTTP_STATUS_ERROR,
                                  LSTR("request timeout in queue"), true);
            continue;
        }

        if (!w && !(w = httpc_pool_get(m))) {
            /* no connection ready. */
            break;
        }
        if (!_G.connection_ready || unlikely(_G.first_connection)) {
            e_info("connected to %*pM:%d", LSTR_FMT_ARG(_G.url), _G.port);
            _G.connection_ready = true;
            _G.first_connection = false;
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

    switch (status) {
      case HTTPC_STATUS_OK:
        python_http_process_answer(q);
        break;

      case HTTPC_STATUS_ABORT:
      case HTTPC_STATUS_TIMEOUT:
        python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_ERROR,
                              LSTR("HTTP request could not complete"), true);
        break;

      case HTTPC_STATUS_INVALID:
      case HTTPC_STATUS_TOOLARGE:
      default:
        python_http_query_end(&q->ctx, PYTHON_HTTP_STATUS_ERROR,
                              LSTR("unable to parse server response"), true);
        break;
    }
    python_query_delete(&q);
    process_queries(_G.m, NULL);
}

static void python_http_on_term(el_t idx, int signum, el_data_t priv)
{
    el_unregister(&_G.blocker);
}

static void net_rtcl_on_ready(net_rctl_t *rctl)
{
    process_queries(_G.m, NULL);
}

static void http_on_connect_error(const httpc_t *httpc, int errnum)
{
    if (_G.connection_ready || _G.first_connection) {
        e_error("fail to connect to %*pM:%d: %s",
                LSTR_FMT_ARG(_G.url), _G.port, strerror(errnum));
        _G.connection_ready = false;
        _G.first_connection = false;
    }
}

/* }}}*/
/* public interface {{{*/

static PyObject *python_http_initialize(PyObject *self, PyObject *args)
{
    t_scope;
    PyObject *cb_build_headers = NULL;
    PyObject *cb_build_body    = NULL;
    PyObject *cb_parse         = NULL;

    struct core__httpc_cfg__t iop_cfg;
    httpc_cfg_t              *cfg     = NULL;
    char                     *cfg_hex = NULL;
    lstr_t                    cfg_lstr;
    size_t                    cfg_str_len;

    sockunion_t su;
    int         maxconn  = PYTHON_HTTP_MAXCONN_DEFAULT;
    int         maxrate  = PYTHON_HTTP_MAXRATE_DEFAULT;
    char       *url      = NULL;
    char       *url_arg  = NULL;
    char       *user     = NULL;
    char       *password = NULL;

    _G.queue_timeout = 0;
    if (args == NULL
    ||  !PyArg_ParseTuple(args, "sOOOszii|iiIzz",
                          &cfg_hex,
                          &cb_build_headers,
                          &cb_build_body,
                          &cb_parse,
                          &url,
                          &url_arg,
                          &_G.port,
                          &_G.http_method,
                          &maxconn,
                          &maxrate,
                          &_G.queue_timeout,
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

    cfg_str_len = strlen(cfg_hex) / 2;
    cfg_lstr = LSTR_INIT_V(t_new_raw(char, cfg_str_len), cfg_str_len);
    if (strconv_hexdecode((void *)cfg_lstr.s, cfg_lstr.len,
                           cfg_hex, strlen(cfg_hex)) < 0)
    {
        PyErr_SetString(http_initialize_error,
                        "failed to hex decode httpc cfg");
        return NULL;
    }
    if (t_iop_bunpack(&cfg_lstr, core__httpc_cfg, &iop_cfg) < 0) {
        PyErr_SetString(http_initialize_error, "failed to unpack httpc cfg");
        return NULL;
    }

    _G.url = lstr_dups(url, strlen(url));
    _G.url_args = lstr_dups(url_arg, strlen(url_arg));

    if (addr_info_str(&su, _G.url.s, _G.port, AF_UNSPEC) < 0) {
        PyErr_Format(http_initialize_error,
                     "unable to resolve: %s", _G.url.s);
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

    _G.user     = LSTR(user);
    _G.password = LSTR(password);

    _G.m = httpc_pool_new();
    _G.m->su               = su;
    _G.m->on_ready         = process_queries;
    _G.m->host             = lstr_fmt("%s:%d", _G.url.s, _G.port);
    _G.m->cfg              = cfg;
    _G.m->max_len          = maxconn;
    _G.m->on_connect_error = http_on_connect_error;

    _G.pool = mem_fifo_pool_new("python-http", PYTHON_HTTP_POOL_SIZE);
    dlist_init(&_G.pending);

    net_rctl_init(&_G.rctl, maxrate, net_rtcl_on_ready);
    net_rctl_start(&_G.rctl);

    _G.first_connection = true;

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

static PyObject *
python_http_query(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static const char *kwlist[] = {"data", "query_done_cb", "url_path",
                                   "url_args", "http_method", "encode_url",
                                   NULL};
    PyObject                 *data = NULL;
    PyObject                 *cb_query_done = NULL;
    char                     *path = NULL;
    char                     *url_args = NULL;
    core__iop_http_method__t http_method = _G.http_method;
    bool encode_url = true;
    python_ctx_t *ctx;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOz|zib", (char **)kwlist,
                                     &data, &cb_query_done, &path, &url_args,
                                     &http_method, &encode_url))
    {
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
    ctx->path = path? lstr_dups(path, strlen(path)) : LSTR_NULL_V;
    ctx->url_args = url_args ? lstr_dups(url_args, strlen(url_args))
                             : LSTR_NULL_V;
    ctx->http_method = http_method;
    ctx->encode_url = encode_url;

    if (_G.nb_pending > PYTHON_HTTP_MAX_PENDING) {
        python_http_query_end(&ctx, PYTHON_HTTP_STATUS_ERROR,
                              LSTR("too many requests in queue"), false);
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
/* {{{ logger */

#define PY_LOG_PANIC  10

struct pylogger_object_t {
    PyObject_HEAD
    PyObject *fields;
    logger_t *logger;
};

static void pylogger_wipe(pylogger_object_t **pyobj)
{
    logger_delete(&(*pyobj)->logger);
    Py_DECREF((*pyobj));
}

static PyObject *pylogger_log(pylogger_object_t *self, PyObject *args)
{
    const char *str = NULL;
    int         log_level;

    if (!PyArg_ParseTuple(args, "is", &log_level, &str)) {
        PyErr_Format(PyExc_RuntimeError,
                     "invalid argument, expect (log_level, message)");
        return NULL;
    }
    switch (log_level) {
      case LOG_CRIT:
      case LOG_ERR:
      case LOG_WARNING:
      case LOG_NOTICE:
      case LOG_INFO:
      case LOG_DEBUG:
        logger_log(self->logger, log_level, "%s", str);
        break;

      case PY_LOG_PANIC:
        logger_panic(self->logger, "%s", str);
        break;

      default:
        PyErr_Format(PyExc_RuntimeError, "invalid log level: %d", log_level);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
pylogger_new(PyTypeObject *type, PyObject *args, PyObject *kw)
{
    pylogger_object_t *self;

    self = (pylogger_object_t *)type->tp_alloc(type, 0);
    return (PyObject *)self;
}

static int
pylogger_init(pylogger_object_t *self, PyObject *args, PyObject *kw)
{
    t_scope;
    logger_t *parent_logger = NULL;
    lstr_t parent = LSTR_NULL;
    lstr_t name = LSTR_NULL;
    lstr_t fullname;
    int silent = 0;

    if (!PyArg_ParseTuple(args, "z#s#|i", &parent.s, &parent.len,
                          &name.s, &name.len, &silent)) {
        PyErr_Format(PyExc_RuntimeError,
                     "invalid arguments, expect (parent, name, [silent])");
        return -1;
    }

    if (!name.len) {
        PyErr_Format(PyExc_RuntimeError, "logger name cannot be empty");
        return -1;
    }

    if (parent.len) {
        parent_logger = logger_get_by_name(parent);
        if (!parent_logger) {
            PyErr_Format(PyExc_RuntimeError, "unknown parent logger `%s`",
                         parent.s);
            return -1;
        }
    }
    if (parent_logger) {
        fullname = t_lstr_fmt("%*pM/%*pM",
                              LSTR_FMT_ARG(parent_logger->full_name),
                              LSTR_FMT_ARG(name));
    } else {
        fullname = name;
    }
    if (logger_get_by_name(fullname)) {
        PyErr_Format(PyExc_RuntimeError, "logger `%s` already exists",
                     fullname.s);
        return -1;
    }
    self->logger = logger_new(parent_logger, name, LOG_INHERITS,
                              silent ? LOG_SILENT : 0);
    if (_G.pylogger_class_initialized) {
        Py_INCREF(self);
        qh_add(pylogger, &_G.pyloggers, self);
    }
    return 0;
}

static void
pylogger_dealloc(pylogger_object_t *self)
{
    qh_deep_del_key(pylogger, &_G.pyloggers, self, pylogger_wipe);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMethodDef pylogger_methods[] = {
    {"log", (PyCFunction)pylogger_log, METH_VARARGS,
        "log(level, message): emit a log using the specified log level"},
    {NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(pylogger_doc,
"The goal of this class is to bind lib-common C logger to python.\n"
"The class constructor takes 3 arguments:\n"
"- parent: name of the parent logger, can be None\n"
"- name: logger name,\n"
"- silent: optional boolean to create silent logger\n");

PyTypeObject pylogger_class = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name       = "common.Logger",
    .tp_basicsize  = sizeof(pylogger_object_t),
    .tp_flags      = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_methods    = pylogger_methods,
    .tp_dealloc    = (destructor)pylogger_dealloc,
    .tp_init       = (initproc)pylogger_init,
    .tp_new        = pylogger_new,
    .tp_doc        = pylogger_doc
};

PyTypeObject *pylogger_class_init(void)
{
    RETHROW_NP(PyType_Ready(&pylogger_class));
    _G.pylogger_class_initialized = true;
    qh_init(pylogger, &_G.pyloggers);
    return &pylogger_class;
}

void pylogger_class_wipe(void)
{
    if (_G.pylogger_class_initialized) {
        _G.pylogger_class_initialized = false;
        qh_deep_wipe(pylogger, &_G.pyloggers, pylogger_wipe);
    }
}

/* }}} */
/* {{{ python module */

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
        "http_initialize",
        (PyCFunction)python_http_initialize,
        METH_VARARGS,
        "initialize http"
    },
    {
        "http_query",
        (PyCFunction) python_http_query,
        METH_VARARGS|METH_KEYWORDS,
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

void py_add_module_constant(PyObject *module, const char *constant_name,
                            long value)
{
    PyObject *py_value = PyLong_FromLong(value);
    PyObject_SetAttrString(module, constant_name, py_value);
    Py_XDECREF(py_value);
}

void py_add_log_constants(PyObject *module)
{
    py_add_module_constant(module, "LOG_CRIT", LOG_CRIT);
    py_add_module_constant(module, "LOG_ERR", LOG_ERR);
    py_add_module_constant(module, "LOG_WARNING", LOG_WARNING);
    py_add_module_constant(module, "LOG_NOTICE", LOG_NOTICE);
    py_add_module_constant(module, "LOG_INFO", LOG_INFO);
    py_add_module_constant(module, "LOG_DEBUG", LOG_DEBUG);
    py_add_module_constant(module, "LOG_PANIC", PY_LOG_PANIC);
}

PyObject *python_common_initialize(const char *name, PyMethodDef methods[])
{
    PyObject *common_module;
    qv_t(PyMethodDef) vec;
    int pos = 0;

    RETHROW_NP(PyType_Ready(&pylogger_class));

    qv_init(&vec);
    p_copy(qv_growlen(&vec, countof(myModule_methods)),
           myModule_methods, countof(myModule_methods));

    do {
        qv_append(&vec, methods[pos]);
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
    http_build_part_error = PyErr_NewException((char *) "http.buildPartError",
                                               NULL, NULL);
    Py_XINCREF(http_build_part_error);
    PyModule_AddObject(common_module, "buildPartError", http_build_part_error);

    Py_INCREF(&pylogger_class);
    PyModule_AddObject(common_module, "Logger", (PyObject *)&pylogger_class);


    py_add_log_constants(common_module);
    py_add_module_constant(common_module, "HTTP_OK",
                           PYTHON_HTTP_STATUS_OK);
    py_add_module_constant(common_module, "HTTP_ERROR",
                           PYTHON_HTTP_STATUS_ERROR);
    py_add_module_constant(common_module, "HTTP_USERERROR",
                           PYTHON_HTTP_STATUS_USERERROR);
    py_add_module_constant(common_module, "HTTP_BUILD_QUERY_ERROR",
                           PYTHON_HTTP_STATUS_BUILD_QUERY_ERROR);

    return common_module;
}

/* }}} */
/* {{{ Python Event loop */
/* {{{ Helpers */

static void py_el_wipe(void **ptr)
{
    el_t *el = (el_t *)ptr;
    data_t data = el_unregister(el);
    PyObject *py_el = data.ptr;

    assert (py_el);
    Py_DECREF(py_el);
}

static void py_el_on_cb_exception(el_t el)
{
    if (_G.py_el_cfg.on_cb_exception) {
        (*_G.py_el_cfg.on_cb_exception)(el);
    }
}

static int py_el_check_all_registered(void)
{
    if (!_G.py_el_module_registered || !_G.py_el_types_registered) {
        PyErr_SetString(PyExc_RuntimeError, "Python event loop module has "
                        "not been correctly initialized");
        return -1;
    }

    return 0;
}

#ifdef IS_PY3K
# define PYINT_FROMLONG(x)        PyLong_FromLong(x)
# define PYSTR_FROMSTR(x)         PyUnicode_FromString(x)
# define PYSTR_FROMSTRSIZE(x, l)  PyUnicode_FromStringAndSize(x, l)
#else
# define PYINT_FROMLONG(x)        PyInt_FromLong(x)
# define PYSTR_FROMSTR(x)         PyString_FromString(x)
# define PYSTR_FROMSTRSIZE(x, l)  PyString_FromStringAndSize(x, l)
#endif

/* }}} */
/* {{{ Types */
/* {{{ PyElBase */

#define PY_ELBASE_FIELDS                                                     \
    el_t el;                                                                 \
    PyObject *handler

typedef struct PyElBase {
    PyObject_HEAD
    PY_ELBASE_FIELDS;
} PyElBase;

static void py_el_unregister(PyElBase *py_el)
{
    qh_deep_del_key(ptr, &_G.py_els, py_el->el, py_el_wipe);
    py_el->el = NULL;
}

static int PyElBase_traverse(PyElBase *self, visitproc visit, void *arg)
{
    Py_VISIT(self->handler);
    return 0;
}

static int PyElBase_clear(PyElBase *self)
{
    Py_CLEAR(self->handler);
    return 0;
}

static void PyElBase_dealloc(PyElBase *self)
{
    PyElBase_clear(self);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

PyDoc_STRVAR(PyElBase_handler_doc, "Handler called on event.");

static PyMemberDef PyElBase_members[] = {
    {
        (char *)"handler",
        T_OBJECT_EX,
        offsetof(PyElBase, handler),
        READONLY,
        PyElBase_handler_doc
    }, {
        NULL,
        0,
        0,
        0,
        NULL
    }
};

PyDoc_STRVAR(PyElBase_unregister_doc, "Unregister the event.");

static PyObject *PyElBase_unregister(PyElBase *self, PyObject *null)
{
    py_el_unregister(self);
    Py_RETURN_NONE;
}

static PyMethodDef PyElBase_methods[] = {
    {
        "unregister",
        (PyCFunction)PyElBase_unregister,
        METH_NOARGS,
        PyElBase_unregister_doc
    }, {
        NULL,
        NULL,
        0,
        NULL
    }
};

PyDoc_STRVAR(PyElBase_type_doc,
    "Event loop object\n"
    "\n"
    "Contains a reference to the C event variable.");

static PyTypeObject PyElBase_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "ElBase",
    .tp_basicsize = sizeof(PyElBase),
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = PyElBase_type_doc,
    .tp_methods   = PyElBase_methods,
    .tp_members   = PyElBase_members,
    .tp_dealloc   = (destructor)PyElBase_dealloc,
    .tp_clear     = (inquiry)PyElBase_clear,
    .tp_traverse  = (traverseproc)PyElBase_traverse,
};

/* }}} */
/* {{{ PyElFd */

PyDoc_STRVAR(PyElFd_get_fd_doc,
             "Get the corresponding file descriptor of the fd event.");

static PyObject *PyElFd_get_fd(PyElBase *self, PyObject *null)
{
    int fd = el_fd_get_fd(self->el);

    return PYINT_FROMLONG(fd);
}

static PyMethodDef PyElFd_methods[] = {
    {
        "get_fd",
        (PyCFunction)PyElFd_get_fd,
        METH_NOARGS,
        PyElFd_get_fd_doc
    }, {
        NULL,
        NULL,
        0,
        NULL
    }
};

PyDoc_STRVAR(PyElFd_type_doc,
    "Fd Event loop object\n"
    "\n"
    "Contains a reference to the C fd event variable.");

static PyTypeObject PyElFd_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "ElFd",
    .tp_base      = &PyElBase_type,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = PyElFd_type_doc,
    .tp_methods   = PyElFd_methods,
};

/* }}} */
/* {{{ PyElTimer */

#define PY_ELTIMER_FIELDS                                                    \
    PY_ELBASE_FIELDS;                                                        \
    bool is_armed : 1

typedef struct PyElTimer {
    PyObject_HEAD
    PY_ELTIMER_FIELDS;
} PyElTimer;

PyDoc_STRVAR(PyElTimer_is_repeated_doc,
             "True if the corresponding el timer is repeated.");

static PyObject *PyElTimer_is_repeated(PyElTimer *self, PyObject *null)
{
    if (el_timer_is_repeated(self->el)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

PyDoc_STRVAR(PyElTimer_restart_doc,
    "Restart a one-shot timer.\n"
    "\n"
    "Note that if the timer hasn't expired yet, it just sets it to a later "
    "time.\n"
    "\n"
    "Arguments:\n"
    "    [next]: relative time in ms at which the timer will fire. If not \n"
    "set or negative, the previous relative value is reused."
);

static PyObject *PyElTimer_restart(PyElTimer *self, PyObject *args,
                                   PyObject *kwargs)
{
    static const char *kwlist[] = {"next", NULL};
    int64_t next = -1;

    if (!self->el) {
        PyErr_SetString(PyExc_TypeError, "timer has been unregistered");
        return NULL;
    }

    if (el_timer_is_repeated(self->el)) {
        PyErr_SetString(PyExc_TypeError, "timer is not a one-shot timer");
        return NULL;
    }

    THROW_NULL_UNLESS(PyArg_ParseTupleAndKeywords(args, kwargs, "|L",
                                                  (char **)kwlist, &next));
    el_timer_restart(self->el, next);
    self->is_armed = true;
    Py_RETURN_NONE;
}

static PyMethodDef PyElTimer_methods[] = {
    {
        "is_repeated",
        (PyCFunction)PyElTimer_is_repeated,
        METH_NOARGS,
        PyElTimer_is_repeated_doc
    }, {
        "restart",
        (PyCFunction)PyElTimer_restart,
        METH_VARARGS | METH_KEYWORDS,
        PyElTimer_restart_doc
    }, {
        NULL,
        NULL,
        0,
        NULL
    }
};

PyDoc_STRVAR(PyElTimer_type_doc,
    "Timer Event loop object\n"
    "\n"
    "Contains a reference to the C timer event variable.");

static PyTypeObject PyElTimer_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "ElTimer",
    .tp_basicsize = sizeof(PyElTimer),
    .tp_base      = &PyElBase_type,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = PyElTimer_type_doc,
    .tp_methods   = PyElTimer_methods,
};

/* }}} */
/* {{{ PyElFs */

#define PY_ELFSWATCH_FIELDS                                                  \
    PY_ELBASE_FIELDS;                                                        \
    uint32_t flags

typedef struct PyElFsWatch {
    PyObject_HEAD
    PY_ELFSWATCH_FIELDS;
} PyElFsWatch;

PyDoc_STRVAR(PyElFsWatch_flags_doc, "The events to watch.");

static PyMemberDef PyElFsWatch_members[] = {
    {
        (char *)"flags",
        T_OBJECT_EX,
        offsetof(PyElFsWatch, flags),
        READONLY,
        PyElFsWatch_flags_doc
    }, {
        NULL,
        0,
        0,
        0,
        NULL
    }
};

PyDoc_STRVAR(PyElFsWatch_change_doc,
             "Change the events to watch.\n"
             "\n"
             "Arguments:\n"
             "    flags: the events to watch.");

static PyObject *PyElFsWatch_change(PyElFsWatch *self, PyObject *args,
                                    PyObject *kwargs)
{
    static const char *kwlist[] = {"flags", NULL};
    uint32_t flags;

    if (!self->el) {
        PyErr_SetString(PyExc_TypeError, "fs watch has been unregistered");
        return NULL;
    }

    THROW_NULL_UNLESS(PyArg_ParseTupleAndKeywords(args, kwargs, "I",
                                                  (char **)kwlist, &flags));

    if (el_fs_watch_change(self->el, flags) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "error while changing events of "
                        "fs watch");
        return NULL;
    }
    self->flags = flags;
    Py_RETURN_NONE;
}

PyDoc_STRVAR(PyElFsWatch_get_path_doc,
             "Get path of the watched directory/file.");

static PyObject *PyElFsWatch_get_path(PyElFsWatch *self, PyObject *null)
{
    const char *path;

    if (!self->el) {
        PyErr_SetString(PyExc_TypeError, "fs watch has been unregistered");
        return NULL;
    }

    path = el_fs_watch_get_path(self->el);
    return PYSTR_FROMSTR(path);
}

static PyMethodDef PyElFsWatch_methods[] = {
    {
        "change",
        (PyCFunction)PyElFsWatch_change,
        METH_VARARGS | METH_KEYWORDS,
        PyElFsWatch_change_doc
    }, {
        "get_path",
        (PyCFunction)PyElFsWatch_get_path,
        METH_NOARGS,
        PyElFsWatch_get_path_doc
    }, {
        NULL,
        NULL,
        0,
        NULL
    }
};

PyDoc_STRVAR(PyElFsWatch_type_doc,
    "File system watcher event loop object\n"
    "\n"
    "Contains a reference to the C fs watch event variable");

static PyTypeObject PyElFsWatch_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "ElFsWatch",
    .tp_basicsize = sizeof(PyElFsWatch),
    .tp_base      = &PyElBase_type,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_doc       = PyElFsWatch_type_doc,
    .tp_members   = PyElFsWatch_members,
    .tp_methods   = PyElFsWatch_methods,
};

/* }}} */
/* }}} */
/* {{{ Method functions */

static int py_register_fd_cb(el_t el, int fd, short ev, data_t priv)
{
    py_gil_lock_scope;
    PyElBase *py_el = priv.ptr;
    PyObject *res;

    assert (el == py_el->el);
    res = PyObject_CallObject(py_el->handler, NULL);
    if (res) {
        Py_DECREF(res);
    } else {
        py_el_on_cb_exception(el);
    }
    return 0;
}

PyDoc_STRVAR(py_register_el_fd_doc,
    "Register a file descriptor in the event loop.\n"
    "\n"
    "Arguments:\n"
    "    fd: file descriptor to register.\n"
    "    eventmask: bitmask describing the type of events you want to \n"
    "check for. See poll(2) and the select Python module for a description \n"
    "of each event type.\n"
    "    handler: must be a callable Object. Called when a event is \n"
    "received.\n"
    "\n"
    "Returns the ElFd corresponding to the event.\n"
    "\n"
    "Example:\n"
    "class ResponseProcessor(BaseHTTPRequestHandler):\n"
    "    def do_GET(self):\n"
    "        self.send_response(200)\n"
    "        self.end_headers()\n"
    "        self.wfile.write('YEPYEPYEP')\n"
    "\n"
    "\n"
    "class MyMinion(minion.Minion):\n"
    "    def on_ready(self):\n"
    "        server_addr = (self.cfg.asyncServerAddr,\n"
    "                       self.cfg.asyncServerPort)\n"
    "        httpd = HTTPServer(server_addr, ResponseProcessor)\n"
    "        minion.register_el_fd(httpd.fileno(), select.POLLIN,\n"
    "                              httpd.handle_request)\n"
);

static PyObject *py_register_el_fd(PyObject *self, PyObject *args,
                                   PyObject *kwargs)
{
    static const char *kwlist[] = {"fd", "eventmask", "handler", NULL};
    PyObject *handler;
    int fd;
    short eventmask;
    PyElBase *py_el;

    RETHROW_NP(py_el_check_all_registered());

    THROW_NULL_UNLESS(PyArg_ParseTupleAndKeywords(args, kwargs, "ihO",
                                                  (char **)kwlist, &fd,
                                                  &eventmask, &handler));

    py_el = RETHROW_P(PyObject_New(PyElBase, &PyElFd_type));
    Py_INCREF(handler);
    py_el->handler = handler;
    py_el->el = el_fd_register(fd, false, eventmask, &py_register_fd_cb,
                               py_el);
    qh_add(ptr, &_G.py_els, py_el->el);

    Py_INCREF(py_el);
    return (PyObject *)py_el;
}

static void py_register_timer_cb(el_t el, data_t priv)
{
    py_gil_lock_scope;
    PyElTimer *py_el = priv.ptr;
    PyObject *res;

    assert (el == py_el->el);
    if (!el_timer_is_repeated(el)) {
        py_el->is_armed = false;
    }

    Py_INCREF(py_el);
    res = PyObject_CallFunctionObjArgs(py_el->handler, (PyObject *)py_el,
                                       NULL);
    if (res) {
        Py_DECREF(res);
    } else {
        py_el_on_cb_exception(el);
    }

    if (py_el->el && !py_el->is_armed) {
        /* XXX: The one-shot timer was not unregistered but was not rearmed
         * either. It will be automatically unregistered after this callback,
         * so we need to remove it from py_els. */
        py_el_unregister((PyElBase *)py_el);
    }
    Py_DECREF(py_el);
}

PyDoc_STRVAR(py_register_el_timer_doc,
    "Register a timer in the event loop.\n"
    "\n"
    "There are two kinds of timers: one shot and repeating timers:\n"
    "- One shot timers fire their callback once, when they time out.\n"
    "- Repeating timers automatically rearm after being fired.\n"
    "\n"
    "One shot timers are automatically destroyed at the end of the callback\n"
    "if they have not be rearmed in it. As a consequence, you must be\n"
    "careful to cleanup all references to one-shot timers you did not rearm\n"
    "within the callback.\n"
    "\n"
    "Arguments:\n"
    "    next:    relative time in ms at which the timer fires.\n"
    "    repeat:  repeat interval in ms, 0 means single shot.\n"
    "    handler: callback to call upon timer expiry.\n"
    "    [flags]: timer related flags (EL_TIMER_NOMISS, EL_TIMER_LOWRES).\n"
    "\n"
    "Handler arguments:\n"
    "    el: the timer event.\n"
    "\n"
    "Returns the ElTimer corresponding to the event.\n"
    "\n"
    "Example:\n"
    "def print_plop(el):\n"
    "    LOGGER.info('plop')\n"
    "    el.restart(1000)\n"
    "\n"
    "\n"
    "class MyMinion(minion.Minion):\n"
    "    def on_ready(self):\n"
    "        module.register_el_timer(2000, 0, print_plop,\n"
    "                                 module.EL_TIMER_LOWRES)"
);

static PyObject *py_register_el_timer(PyObject *self, PyObject *args,
                                      PyObject *kwargs)
{
    static const char *kwlist[] = {
        "next", "repeat", "handler", "flags", NULL
    };
    int64_t next;
    int64_t repeat;
    PyObject *handler;
    int flags = 0;
    PyElTimer *py_el;

    RETHROW_NP(py_el_check_all_registered());

    THROW_NULL_UNLESS(PyArg_ParseTupleAndKeywords(args, kwargs, "LLO|i",
                                                  (char **)kwlist, &next,
                                                  &repeat, &handler, &flags));

    py_el = RETHROW_P(PyObject_New(PyElTimer, &PyElTimer_type));
    Py_INCREF(handler);
    py_el->handler = handler;
    py_el->el = el_timer_register(next, repeat, flags, &py_register_timer_cb,
                                  py_el);
    py_el->is_armed = true;
    qh_add(ptr, &_G.py_els, py_el->el);

    Py_INCREF(py_el);
    return (PyObject *)py_el;
}

static void py_register_el_fs_watch_cb(el_t el, uint32_t mask,
                                       uint32_t cookie, lstr_t name,
                                       data_t priv)
{
    py_gil_lock_scope;
    PyElFsWatch *py_el = priv.ptr;
    PyObject *py_mask = PYINT_FROMLONG(mask);
    PyObject *py_cookie = PYINT_FROMLONG(cookie);
    PyObject *py_name = PYSTR_FROMSTRSIZE(name.s, name.len);
    PyObject *res;

    assert (el == py_el->el);
    Py_INCREF(py_el);

    res = PyObject_CallFunctionObjArgs(py_el->handler, (PyObject *)py_el,
                                       py_mask, py_cookie, py_name, NULL);
    if (res) {
        Py_DECREF(res);
    } else {
        py_el_on_cb_exception(el);
    }
    Py_DECREF(py_el);
    Py_DECREF(py_mask);
    Py_DECREF(py_cookie);
    Py_DECREF(py_name);
}

PyDoc_STRVAR(py_register_el_fs_watch_doc,
    "Register a file-system watcher in the event loop.\n"
    "\n"
    "Monitor file-system events using inotify(7).\n"
    "\n"
    "Arguments:\n"
    "    path:    the path to watch.\n"
    "    flags:   the events to watch, see inotify(7).\n"
    "    handler: callback to call when the event is triggered.\n"
    "\n"
    "Handler arguments:\n"
    "    el:      the file-system watcher.\n"
    "    mask:    mask describing the event.\n"
    "    cookie:  unique cookie associating related events, see inotify(7).\n"
    "    name:    name of the file in case of directory event.\n"
    "\n"
    "Returns the ElFsWatch corresponding to the event.\n"
    "\n"
    "Example:\n"
    "def watch_cb(el, mask, cookie, name):\n"
    "    LOGGER.info('mask: %d, cookie: %d, name: %s', mask, cookie, name)\n"
    "\n"
    "\n"
    "class MyMinion(minion.Minion):\n"
    "    def on_ready(self):\n"
    "        flags = minion.IN_ONLYDIR | minion.IN_MOVED_TO | "
    "minion.IN_CREATE\n"
    "        module.register_el_fs_watch('/tmp/watched_dir', flags, watch_cb)"
);

static PyObject *py_register_el_fs_watch(PyObject *self, PyObject *args,
                                         PyObject *kwargs)
{
    static const char *kwlist[] = {"path", "flags", "handler", NULL};
    const char *path;
    uint32_t flags;
    PyObject *handler;
    PyElFsWatch *py_el;

    RETHROW_NP(py_el_check_all_registered());

    THROW_NULL_UNLESS(PyArg_ParseTupleAndKeywords(args, kwargs, "sIO",
                                                  (char **)kwlist, &path,
                                                  &flags, &handler));

    py_el = RETHROW_P(PyObject_New(PyElFsWatch, &PyElFsWatch_type));
    Py_INCREF(handler);
    py_el->handler = handler;
    py_el->flags = flags;
    py_el->el = el_fs_watch_register(path, flags, &py_register_el_fs_watch_cb,
                                     py_el);
    if (!py_el->el) {
        Py_DECREF(py_el);
        PyErr_SetString(PyExc_RuntimeError, "error while registering fs "
                        "watch");
        return NULL;
    }

    qh_add(ptr, &_G.py_els, py_el->el);

    Py_INCREF(py_el);
    return (PyObject *)py_el;
}

PyMethodDef python_el_methods_g[] = {
    {
        "register_el_fd",
        (PyCFunction)py_register_el_fd,
        METH_VARARGS | METH_KEYWORDS,
        py_register_el_fd_doc
    }, {
        "register_el_timer",
        (PyCFunction)py_register_el_timer,
        METH_VARARGS | METH_KEYWORDS,
        py_register_el_timer_doc
    }, {
        "register_el_fs_watch",
        (PyCFunction)py_register_el_fs_watch,
        METH_VARARGS | METH_KEYWORDS,
        py_register_el_fs_watch_doc
    }, {
        NULL,
        NULL,
        0,
        NULL,
    }
};

/* }}} */
/* {{{ Module */

static int register_el_py_type(PyObject *module, PyTypeObject *type)
{
    RETHROW(PyType_Ready(type));
    Py_INCREF(type);
    PyModule_AddObject(module, type->tp_name, (PyObject *)type);
    return 0;
}

int register_python_el_types(PyObject *module)
{
    RETHROW(register_el_py_type(module, &PyElBase_type));
    RETHROW(register_el_py_type(module, &PyElFd_type));
    RETHROW(register_el_py_type(module, &PyElTimer_type));
    RETHROW(register_el_py_type(module, &PyElFsWatch_type));

#define ADD_ENUM(_x)  PyModule_AddObject(module, #_x, PYINT_FROMLONG(_x))

    ADD_ENUM(EL_TIMER_NOMISS);
    ADD_ENUM(EL_TIMER_LOWRES);

    ADD_ENUM(IN_ACCESS);
    ADD_ENUM(IN_ATTRIB);
    ADD_ENUM(IN_CLOSE_WRITE);
    ADD_ENUM(IN_CLOSE_NOWRITE);
    ADD_ENUM(IN_CREATE);
    ADD_ENUM(IN_DELETE);
    ADD_ENUM(IN_DELETE_SELF);
    ADD_ENUM(IN_MODIFY);
    ADD_ENUM(IN_MOVE_SELF);
    ADD_ENUM(IN_MOVED_FROM);
    ADD_ENUM(IN_MOVED_TO);
    ADD_ENUM(IN_OPEN);
    ADD_ENUM(IN_IGNORED);
    ADD_ENUM(IN_ISDIR);
    ADD_ENUM(IN_Q_OVERFLOW);
    ADD_ENUM(IN_UNMOUNT);
    ADD_ENUM(IN_ONLYDIR);
    ADD_ENUM(IN_MOVE);
    ADD_ENUM(IN_CLOSE);

#undef ADD_ENUM

    _G.py_el_types_registered = true;
    return 0;
}

static int python_el_initialize(void *arg)
{
    python_el_cfg_t *cfg = arg;

    if (cfg) {
        _G.py_el_cfg = *cfg;
    }
    qh_init(ptr, &_G.py_els);
    _G.py_el_module_registered = true;
    return 0;
}

static void python_el_on_term(int signo)
{
    py_gil_lock_scope;

    qh_deep_wipe(ptr, &_G.py_els, py_el_wipe);
}

static int python_el_shutdown(void)
{
    _G.py_el_module_registered = false;
    return 0;
}

MODULE_BEGIN(python_el)
    MODULE_IMPLEMENTS_INT(on_term, &python_el_on_term);
MODULE_END()

/* }}} */
/* }}} */
