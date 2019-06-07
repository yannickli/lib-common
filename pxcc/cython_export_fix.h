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

#ifndef IS_CYTHON_EXPORT_FIX_H
#define IS_CYTHON_EXPORT_FIX_H

#include <Python.h>

#ifndef EXPORT
#  ifdef __GNUC__
#    define EXPORT  extern __attribute__((visibility("default")))
#  else
#    define EXPORT  extern
#  endif
#endif

#ifndef PyMODINIT_FUNC
#  error "PyMODINIT_FUNC should be defined"
#endif

#undef PyMODINIT_FUNC
#if PY_MAJOR_VERSION < 3
#  define PyMODINIT_FUNC  EXPORT void
#else
#  define PyMODINIT_FUNC  EXPORT PyObject *
#endif

#endif /* IS_CYTHON_EXPORT_FIX_H */
