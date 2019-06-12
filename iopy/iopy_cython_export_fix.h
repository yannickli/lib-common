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

#ifndef IS_IOPY_CYTHON_EXPORT_FIX_H
#define IS_IOPY_CYTHON_EXPORT_FIX_H

#include <Python.h>
#include <lib-common/core.h>
#include <lib-common/iop.h>

/* Fix compile error for missing PyUnicode_AsUTF8AndSize for Python < 3.3. */
#if PY_VERSION_HEX < 0x03030000
#  define PyUnicode_AsUTF8AndSize(...)  ({ assert(false); NULL; })
#endif

/* Export public functions for the DSO. */
EXPORT const iop_struct_t *Iopy_struct_union_type_get_desc(PyObject *);
EXPORT bool Iopy_has_pytype_from_fullname(PyObject *);
EXPORT PyObject *Iopy_get_pytype_from_fullname_(PyObject *, lstr_t);
EXPORT bool Iopy_Struct_to_iop_ptr(mem_pool_t *, void **,
                                   const iop_struct_t *,
                                   PyObject *);
EXPORT bool Iopy_Union_to_iop_ptr(mem_pool_t *, void **, const iop_struct_t *,
                                  PyObject *);
EXPORT void Iopy_add_iop_package(const iop_pkg_t *, PyObject *);
EXPORT int Iopy_remove_iop_package(const iop_pkg_t *, PyObject *);
EXPORT PyObject *Iopy_from_iop_struct_or_union(PyObject *,
                                               const iop_struct_t *,
                                               const void *);
EXPORT PyObject *Iopy_make_plugin_from_handle(void *handle, const char *path);

#if PY_MAJOR_VERSION < 3
EXPORT PyMODINIT_FUNC initiopy(void);
#else
EXPORT PyMODINIT_FUNC PyInit_iopy(void);
#endif

/* These macros are redefined by cython */
#ifdef likely
#  undef likely
#endif
#ifdef unlikely
#  undef unlikely
#endif
#ifdef __unused__
#  undef __unused__
#endif

/* Disable clang comma warnings for Cython >= 3.9 */
#if defined(__clang__)                                                       \
 && (__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 9))
#  pragma GCC diagnostic ignored "-Wcomma"
#endif

#endif /* IS_IOPY_CYTHON_EXPORT_FIX_H */
