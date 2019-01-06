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

#ifndef IS_LIBCOMMON_PYTHON_COMMON
#define IS_LIBCOMMON_PYTHON_COMMON

#include <Python.h>
#include "core.h"
#include "container-qvector.h"
#include "el.h"

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#endif

#ifdef IS_PY3K
#define Py_INITERROR return NULL
#else
#define Py_INITERROR return
#endif

extern PyThreadState *python_state_g;

qvector_t(PyMethodDef, PyMethodDef);
PyObject *python_common_initialize(const char *name, PyMethodDef methods[]);

void py_add_module_constant(PyObject *module, const char *constant_name,
                            long value);
PyTypeObject *pylogger_class_init(void);
void pylogger_class_wipe(void);
void py_add_log_constants(PyObject *module);

/** Add a PyObject to a sb_t.
 *
 * This function adds the string value of a PyObject to a sb_t. It handles
 * Python Bytes, Unicode and all object which can be stringified using
 * PyObject_Str (ie Python str built-in function).
 *
 * \param[out] sb  String buffer in which we append the \p obj string value.
 * \param[in]  obj The PyObject appended to \p sb.
 *
 * \return a negative value in case of error, zero otherwise.
 **/
int sb_add_py_obj(sb_t *sb, PyObject *obj);

/** Fetch a Python exception.
 *
 * This function fetches a Python exception, convert it to string and append
 * it in a sb_t.
 *
 * \param[out] err A pointer on the sb_t filled with the trackback.
 **/
void sb_add_py_traceback(sb_t *err);

/** Call a Python function and catch exception.
 * This macro clears the Python exception and executes The Python expression
 * \p _expr. If an exception is raised (\p _expr returns NULL), it calls
 * sb_add_py_traceback to catch it and convert it to a string, appended in
 * \p _err.
 *
 * \param[in]  _expr A Python expression to execute, typically a
 *                   PyObject_Call* function.
 * \param[out] err   A pointer on the sb_t filled with the trackback.
 *
 * \return the \p _expr result: a Python Object or NULL if an exception is
 * raised.
 * \warning If the PyObject returned is not NULL you have to wipe it with
 * Py_DECREF.
 **/
#define PY_TRY_CATCH(_expr, _err)      \
    ({  PyObject *__res;               \
                                       \
        PyErr_Clear();                 \
        if (!(__res = (_expr))) {      \
            sb_add_py_traceback(_err); \
        }                              \
                                       \
        __res;                         \
    })

/** Ensure that the current thread is ready to call the Python C API in the
 * scope.
 *
 * It uses PyGILState_Ensure() at the beginning of the scope, and
 * PyGILState_Release() at the end.
 */
#define py_gil_lock_scope                                                    \
    __attribute__((unused, cleanup(py_gil_lock_scope_end)))                  \
    PyGILState_STATE py_gstate_scope = PyGILState_Ensure()

static inline void py_gil_lock_scope_end(PyGILState_STATE *gstate)
{
    PyGILState_Release(*gstate);
}

/* {{{ Python Event loop */

/** Module methods of python event loop. */
extern PyMethodDef python_el_methods_g[];

/** Register python types used by event loops functions to given module. */
int register_python_el_types(PyObject *module);

/** Optional configuration for python_el. */
typedef struct python_el_cfg_t {
    /** Callback in case of exception in the el callback.
     *
     * The Python exception is already set when this callback is called.
     */
    void (*on_cb_exception)(el_t el);
} python_el_cfg_t;

/** Module to handle intersec event loop from python. */
MODULE_DECLARE(python_el);

/* }}} */

#endif
