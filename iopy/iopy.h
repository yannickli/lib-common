/***************************************************************************/
/*                                                                         */
/* Copyright 2019 INTERSEC SA                                              */
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

#ifndef IS_IOPY_H
#define IS_IOPY_H

#include <Python.h>
#include <structmember.h>

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#endif

#include <lib-common/core.h>
#include <lib-common/iop.h>

/*----- type declarations -{{{-*/

qvector_t(iop_field, const iop_field_t *);
qm_kvec_t(additional_dso, lstr_t, iop_dso_t *, qhash_lstr_hash,
          qhash_lstr_equal);

PyObject *iopyError;
PyObject *iopyWarning;
PyObject *iopyRpcError;

PyObject *iopy_warningregistry;

PyTypeObject Iopy_Enum_Base_Type;
PyTypeObject Iopy_Enum_Type;
PyTypeObject Iopy_Struct_Union_Base_Type;
PyTypeObject Iopy_Struct_Base_Type;
PyTypeObject Iopy_Struct_Type;
PyTypeObject Iopy_Union_Base_Type;
PyTypeObject Iopy_Union_Type;
PyTypeObject Iopy_RPC_Base_Type;
PyTypeObject Iopy_RPC_Type;
PyTypeObject Iopy_RPC_Server_Type;
PyTypeObject Iopy_Iface_Type;
PyTypeObject Iopy_Module_Type;
PyTypeObject Iopy_Plugin_Type;
PyTypeObject Iopy_Package_Base_Type;
PyTypeObject Iopy_Package_Type;
PyTypeObject Iopy_Channel_Base_Type;
PyTypeObject Iopy_Channel_Type;
PyTypeObject Iopy_Channel_Server_Type;
PyTypeObject Iopy_RPC_Args_Type;

typedef struct Iopy_Enum {
    PyObject_HEAD
    bool iop_initialized;
    PyObject *fields;
    const iop_enum_t *desc;
    int val;
} Iopy_Enum;

typedef struct Iopy_Struct {
    PyObject_HEAD
    bool iop_initialized;
    PyObject *fields;
    const iop_struct_t *desc;
    qv_t(iop_field)     fdescs;
} Iopy_Struct;

typedef struct Iopy_Union {
    PyObject_HEAD
    bool iop_initialized;
    PyObject *fields;
    const iop_struct_t *desc;
    int field_index;
} Iopy_Union;

typedef struct Iopy_RPC {
    PyObject_HEAD
    PyObject *channel;
    PyObject *py_iface;
    const iop_rpc_t *desc;
    const iop_iface_alias_t *iface;
} Iopy_RPC;

typedef struct Iopy_Iface {
    PyObject_HEAD
    PyObject *channel;
    PyObject *funs;
    const iop_iface_alias_t *desc;
} Iopy_Iface;

typedef struct Iopy_Module {
    PyObject_HEAD
    PyObject *channel;
    PyObject *ifaces;
    const iop_mod_t *desc;
    int refcnt;
} Iopy_Module;

typedef struct Iopy_Plugin {
    PyObject_HEAD
    PyObject *dsopath;

    PyObject *modules;
    PyObject *packages;
    PyObject *_register;

    iop_dso_t *dso;
    qm_t(additional_dso) additional_dsos;
} Iopy_Plugin;

typedef struct Iopy_Package {
    PyObject_HEAD
    PyObject *fields;
    const iop_pkg_t *desc;
    int refcnt;
} Iopy_Package;

typedef struct Iopy_Channel {
    PyObject_HEAD
    PyObject *plugin;
    PyObject *modules;
    PyObject *_register;
    struct ic_client_t *ic_client;
    struct ic_server_t *ic_server;
    int timeout;
} Iopy_Channel;

/*----- type declarations -}}}-*/
/*----- python macros -{{{-*/

#define UNDEFINED "undefined"
#define VERBOSE 0

qvector_t(pyobject, PyObject *);

static inline void repr_args_cleanup(qv_t(pyobject) *args)
{
    tab_for_each_entry(obj, args) {
        Py_CLEAR(obj);
    }
}

static inline void pyobject_xdecref(PyObject **o)
{
    Py_XDECREF(*o);
}

#define REPR_ARGS_FRAME                                                   \
    qv_t(pyobject) _tmp_args __attribute__((cleanup(repr_args_cleanup))); \
    PyObject *_repr_buf[100];                                             \
    qv_init_static(&_tmp_args, _repr_buf, countof(_repr_buf));  \
    qv_clear(&_tmp_args);

#ifdef IS_PY3K

#define TRACE(a, ...)                                                        \
    do {                                                                     \
        if (VERBOSE) {                                                       \
            REPR_ARGS_FRAME;                                                 \
            PySys_FormatStdout("%s: " a "\n", __func__, ##__VA_ARGS__);      \
        }                                                                    \
    } while (0)

#define LOG(a, ...)                                                          \
    do {                                                                     \
        if (VERBOSE) {                                                       \
            REPR_ARGS_FRAME;                                                 \
            PySys_FormatStdout("%s: " a "\n", __func__, ##__VA_ARGS__);      \
            PyErr_Print();                                                   \
        }                                                                    \
    } while (0)

#define REPR_FORMAT(fmt, ...)  \
    ({ REPR_ARGS_FRAME; PyUnicode_FromFormat(fmt, ##__VA_ARGS__); })

#define REPR_CONCAT(a, b)  do {                            \
        PyObject **_a = (a);                               \
        PyObject *_b = (b);                                \
        PyObject *_tmp_concat = PyUnicode_Concat(*_a, _b); \
        Py_DECREF(*_a);                                    \
        *_a = _tmp_concat;                                 \
        Py_CLEAR(_b); /* b reference is stolen */          \
    } while (0)

#define REPR_FROMSTRING         PyUnicode_FromString
#define REPR_FROMSTRINGANDSIZE  PyUnicode_FromStringAndSize

#define REPR_TOSTRING(_obj)                                                  \
    ({                                                                       \
        PyObject *_str_res = (PyObject *)(_obj);                             \
        PyObject *_repr = PyObject_Repr(_str_res);                           \
                                                                             \
        _str_res = PyUnicode_AsUTF8String(_repr);                            \
        Py_CLEAR(_repr);                                                     \
        _str_res;                                                            \
    })

#define PyEval_EvalCode(_co, _globals, _locals)  \
    ((PyEval_EvalCode)((PyObject *)(_co), (_globals), (_locals)))

#else

#undef PyErr_BadInternalCall
#define PyErr_BadInternalCall() _PyErr_BadInternalCall((char *)__FILE__, __LINE__)

#define TRACE(...)  do {                             \
        if (unlikely(e_name_is_traced(3, "iopy"))) { \
            REPR_ARGS_FRAME;                         \
            e_named_trace(3, "iopy", ##__VA_ARGS__); \
        }                                            \
    } while (0)

#define LOG(a, ...)  do {                               \
        if (unlikely(e_name_is_traced(1, "iopy"))) {    \
            REPR_ARGS_FRAME;                            \
            e_named_trace(1, "iopy", a, ##__VA_ARGS__); \
        }                                               \
    } while (0)

#define REPR_FORMAT(fmt, ...)  \
    ({ REPR_ARGS_FRAME; PyString_FromFormat(fmt, ##__VA_ARGS__); })

#define REPR_CONCAT(a, b)  do {                   \
        PyObject *_b = (b);                       \
        PyString_Concat((a), _b);                 \
        Py_CLEAR(_b); /* b reference is stolen */ \
    } while (0)

#define REPR_FROMSTRING         PyString_FromString
#define REPR_FROMSTRINGANDSIZE  PyString_FromStringAndSize

#define REPR_TOSTRING(_obj)  PyObject_Repr((PyObject *)(_obj))

#endif

#define REPR  "%s"
#define REPR_ARG(a)  \
    PyBytes_AsString(qv_append(&_tmp_args, REPR_TOSTRING(a)))

/** XXX: to use when the argument is a function call that build a new python
 *  object which will not be used after */
#define REPR_ARG_TMP_OBJ(X)  ({                                        \
        PyObject *_obj_tmp = (X) ?: REPR_FROMSTRING("INTERNAL ERROR"); \
        const char *_res = REPR_ARG(_obj_tmp);                         \
        Py_CLEAR(_obj_tmp);                                            \
        _res;                                                          \
    })

#define REPR_SET_SB(sb, fmt, ...)  do {  \
        REPR_ARGS_FRAME;                 \
        sb_setf(sb, fmt, ##__VA_ARGS__); \
    } while (0)

#define PYBYTES_GET_CSTRING(o)  ({                                           \
        PyObject *_o = (o);                                                  \
        _o ? PyBytes_AsString(_o) : NULL;                                    \
   })

#define LSTR_TO_PYTHON(_s)  ({                        \
        lstr_t __s = (_s);                            \
        REPR_FROMSTRINGANDSIZE(__s.s ?: "", __s.len); \
    })

#define DECLARE_THIS(Type)  Iopy_##Type *this = ({                           \
        assert (PyType_IsSubtype(Py_TYPE(self), Iopy_##Type##_Type.tp_base));\
        (Iopy_##Type *)self;                                                 \
    })

#define Iopy_Struct_or_Union_desc(Obj)  ({           \
        STATIC_ASSERT (offsetof(Iopy_Struct, desc)   \
                   ==  offsetof(Iopy_Union,  desc)); \
        ((Iopy_Struct *)(Obj))->desc;                \
    })

#define Iopy_Obj_iop_initialized(Obj)  ({                                    \
        STATIC_ASSERT (offsetof(Iopy_Struct, iop_initialized)                \
                   ==  offsetof(Iopy_Union,  iop_initialized));              \
        STATIC_ASSERT (offsetof(Iopy_Struct, iop_initialized)                \
                   ==  offsetof(Iopy_Enum,   iop_initialized));              \
        ((Iopy_Struct *)(Obj))->iop_initialized;                             \
    })

#define ERROR(...)      \
    PyErr_SetString(iopyError, ##__VA_ARGS__)

#define P_ERROR(fmt, ...)  do {                               \
        PyObject *_obj_err = REPR_FORMAT(fmt, ##__VA_ARGS__); \
        PyErr_SetObject(iopyError, _obj_err);                 \
        Py_CLEAR(_obj_err);                                   \
    } while (0)

/* Declare PyObject automatically xdecrefed when getting out of the current
 * scope.
 */
#define PYOBJ_SCOPE(_var, ...)                                               \
    PyObject *_var __attribute__((cleanup(pyobject_xdecref))) = ({           \
        PyObject *_var_vals[] = { NULL, ##__VA_ARGS__ };                     \
        STATIC_ASSERT(countof(_var_vals) <= 2);                              \
        _var_vals[countof(_var_vals) - 1];                                   \
    })

#define main_thread_warn(fmt, ...)  do {                                 \
        t_scope;                                                         \
        PyErr_WarnExplicit(iopyWarning, t_fmt(fmt, ##__VA_ARGS__),       \
                           "sys", 1, NULL, iopy_warningregistry);        \
    } while (0)

#define PYERR_PREPEND(fmt, ...) do {                                     \
        t_scope;                                                         \
        lstr_t lerr = t_pyerr_fetch_err();                               \
        lstr_t res;                                                      \
                                                                         \
        res = t_lstr_fmt(fmt "%*pM", ##__VA_ARGS__,                      \
                          LSTR_FMT_ARG(lerr));                           \
        PyErr_SetString(iopyError, res.s);                               \
    } while (0)

/*----- python macros -}}}-*/
/*----- inline functions -{{{-*/

static inline bool pyobject_is_integer(const PyObject *obj)
{
    return
#ifndef IS_PY3K
           PyType_IsSubtype(Py_TYPE(obj), &PyInt_Type) ||
#endif
           PyType_IsSubtype(Py_TYPE(obj), &PyLong_Type);
}

static inline
int pyobject_get_integer(PyObject *obj, bool is_signed, uint64_t *out)
{
#ifndef IS_PY3K
    if (PyType_IsSubtype(Py_TYPE(obj), &PyInt_Type)) {
        *out = PyInt_AS_LONG(obj);
        return 0;
    }
#endif
    if (PyType_IsSubtype(Py_TYPE(obj), &PyLong_Type)) {
        *out = is_signed ? (uint64_t)PyLong_AsLong(obj)
                         : PyLong_AsUnsignedLong(obj);
        return 0;
    }
    return -1;
}

static inline int pyobject_get_long(PyObject *obj, int64_t *out)
{
    return pyobject_get_integer(obj, true, (uint64_t *)out);
}

static inline int pyobject_get_ulong(PyObject *obj, uint64_t *out)
{
    return pyobject_get_integer(obj, false, out);
}

static inline PyObject *pyobject_get_pybytes(PyObject *o)
{
    if (PyBytes_Check(o)) {
        Py_INCREF(o);
        return o;
    }

    if (PyUnicode_Check(o)) {
        return PyUnicode_AsUTF8String(o);
    }

    return NULL;
}

static inline int pyobject_get_bool(PyObject *o, bool *b)
{
    if (PyBool_Check(o)) {
        *b = (o == Py_True);
        return 0;
    }

    return -1;
}

/*----- inline functions -}}}-*/
/*----- functions declarations -{{{-*/

PyObject *Iopy_Union_new(PyTypeObject *type, const iop_struct_t *desc);
int Iopy_Union_pre_init(PyObject *self);
int Iopy_Union_init(PyObject *self, PyObject *args, PyObject *kwargs);
PyObject *Iopy_Union_set(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *Iopy_Struct_new(PyTypeObject *type, const iop_struct_t *desc);
int Iopy_Struct_pre_init(PyObject *self);
int Iopy_Struct_init(PyObject *self, PyObject *args, PyObject *kwargs);
PyObject *Iopy_Struct_call(PyObject *self, PyObject *args, PyObject *kwargs);

PyObject *Iopy_RPC_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);

PyObject *
Iopy_Channel_build_client(Iopy_Plugin *, PyObject *args, PyObject *kwargs);

PyObject *Iopy_Module_new(PyTypeObject *, PyObject *args, PyObject *kwargs);
PyObject *Iopy_Module_build_funs(PyObject *self);

PyObject *Iopy_pretty_desc(const iop_struct_t *);
PyObject* Iopy_default_init(PyObject *self);

const iop_struct_t *Iopy_struct_union_type_get_desc(PyObject *pytype);

PyObject *
Iopy_from_iop_struct_or_union(PyTypeObject *, const iop_struct_t *,
                              const void *val);
bool
t_iopy_struct_union_to_iop(PyObject *obj, const iop_struct_t *, void **out);
bool Iopy_Struct_to_iop_ptr(mem_pool_t *, void **, const iop_struct_t *,
                            PyObject *self);
bool Iopy_Union_to_iop_ptr(mem_pool_t *, void **, const iop_struct_t *,
                           PyObject *self);

int Iopy_disallow_SetAttr(PyObject *self, PyObject *name, PyObject *value);

int py_dict_put_key_and_value(PyObject *dict, PyObject *key, PyObject *value);
int py_dict_put_value(PyObject *dict, const char *key, PyObject *value);

bool Iopy_has_pytype_from_fullname(PyObject *);
PyTypeObject *Iopy_get_pytype_from_fullname_(PyObject *, lstr_t fullname);
PyTypeObject *
Iopy_get_pytype_from_fullname(PyObject *, lstr_t fullname,
                              PyTypeObject *default_type);
void Iopy_add_iop_package(const iop_pkg_t *p, Iopy_Plugin *plugin);
int Iopy_remove_iop_package(const iop_pkg_t *p, Iopy_Plugin *plugin);

#define Iopy_get_pytype_from_desc(_obj, _desc, _type)                    \
    Iopy_get_pytype_from_fullname((PyObject *)(_obj), (_desc)->fullname, \
                                  (_type))

lstr_t t_iopy_lstr_remove_dots(lstr_t in);

void iopy_rpc_channel_init(PyObject *module);

/** Get the current exception text and clear it if set.
 */
lstr_t t_pyerr_fetch_err(void);

/*----- functions declarations -}}}-*/

#endif
