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

#ifndef IS_LIB_COMMON_BLOB_H
#define IS_LIB_COMMON_BLOB_H

#include "core.h"

/**************************************************************************/
/* Blob printf functions                                                  */
/**************************************************************************/

int blob_pack(sb_t *blob, const char *fmt, ...);
int blob_unpack(const sb_t *blob, int *pos, const char *fmt, ...)
    __must_check__;
int buf_unpack(const byte *buf, int buf_len, int *pos, const char *fmt, ...)
    __must_check__;

#endif /* IS_LIB_COMMON_BLOB_H */
