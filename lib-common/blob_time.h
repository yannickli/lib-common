/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_BLOB_TIME_H
#define IS_LIB_COMMON_BLOB_TIME_H

#include <time.h>

#include "blob.h"

ssize_t blob_strftime(blob_t *blob, ssize_t pos, const char *fmt,
                      const struct tm *tm);

static inline void blob_strftime_utc(blob_t *blob, ssize_t pos, time_t timer)
{
    struct tm tm;

    blob_strftime(blob, pos, "%a, %d %b %Y %H:%M:%S GMT",
                  gmtime_r(&timer, &tm));
}
#endif /* IS_LIB_COMMON_BLOB_TIME_H */
