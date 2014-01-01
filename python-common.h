/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2014 INTERSEC SA                                   */
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

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#endif

#ifdef IS_PY3K
#define Py_INITERROR return NULL
#else
#define Py_INITERROR return
#endif

extern PyThreadState *python_state_g;

PyObject *python_common_initialize(const char *name, PyMethodDef methods[]);

#endif
