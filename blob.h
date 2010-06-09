/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
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

#if 0
#define blob_check_slop()   assert (__sb_slop[0] == '\0')
#else
#define blob_check_slop()
#endif

/**************************************************************************/
/* Blob printf functions                                                  */
/**************************************************************************/

int blob_pack(sb_t *blob, const char *fmt, ...);
int blob_unpack(const sb_t *blob, int *pos, const char *fmt, ...)
    __must_check__;
int buf_unpack(const byte *buf, int buf_len, int *pos, const char *fmt, ...)
    __must_check__;
int blob_serialize(sb_t *blob, const char *fmt, ...)
    __attr_printf__(2, 3);
int buf_deserialize(const byte *buf, int buf_len,
                    int *pos, const char *fmt, ...);
int blob_deserialize(const sb_t *blob, int *pos, const char *fmt, ...);

/**************************************************************************/
/* Blob encoding                                                          */
/**************************************************************************/

void blob_append_wbxml_href(sb_t *dst, const byte *data, int len);

void blob_append_date_iso8601(sb_t *dst, time_t date);

/* in blob_ebcdic.c */
int blob_decode_ebcdic297(sb_t *dst, const char *src, int len);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_make_blob_suite(void);
Suite *check_append_blob_ebcdic_suite(Suite *blob_suite);
Suite *check_append_blob_wbxml_suite(Suite *blob_suite);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_BLOB_H */
