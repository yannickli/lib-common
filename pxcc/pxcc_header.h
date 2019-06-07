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

#ifndef IS_PXCC_HEADER_H
#define IS_PXCC_HEADER_H

#define __PXCC_EXPORT_FILE(x, l)                                             \
    static const char *pxcc_exported_file_ ## l = (x)
#define _PXCC_EXPORT_FILE(x, l)   __PXCC_EXPORT_FILE(x, l)
#define PXCC_EXPORT_FILE(x)      _PXCC_EXPORT_FILE(x, __LINE__)

#define __PXCC_EXPORT_TYPE(x, l)  static x *pxcc_exported_type_ ## l
#define _PXCC_EXPORT_TYPE(x, l)   __PXCC_EXPORT_TYPE(x, l)
#define PXCC_EXPORT_TYPE(x)      _PXCC_EXPORT_TYPE(x, __LINE__)

#define __PXCC_EXPORT_SYMBOL(x, l)                                           \
    static void *pxcc_exported_symbol_ ## l = (void *)&(x)
#define _PXCC_EXPORT_SYMBOL(x, l)  __PXCC_EXPORT_SYMBOL(x, l)
#define PXCC_EXPORT_SYMBOL(x)      _PXCC_EXPORT_SYMBOL(x, __LINE__)

/* Remove _Nonull, _Null_unspecified and _Nullable because of some issues with
 * clang 3.4 */
#define _Nonnull
#define _Null_unspecified
#define _Nullable

#endif /* IS_PXCC_HEADER_H */
