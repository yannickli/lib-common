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

#ifndef IS_LIB_COMMON_ARCHIVE_H
#define IS_LIB_COMMON_ARCHIVE_H

#include <fcntl.h>

#include "mmappedfile.h"
#include "blob.h"

#ifndef IS_LIB_ISDB_INTERNAL
typedef struct isdb_t isdb_t;
#else
#define isdb_t void
#endif

enum {
    DB_PUTOVER,
    DB_PUTKEEP,
    DB_PUTMULT,
};

int db_error(void);
const char *db_strerror(int code);

isdb_t *db_open(const char *name, int flags, int oflags, int bnum);
int db_flush(isdb_t *);
int db_close(isdb_t **);

int db_get(isdb_t *, const char *k, int kl, blob_t *out);
int db_getbuf(isdb_t *, const char *k, int kl, int start, int len, char *buf);
int db_put(isdb_t *, const char *k, int kl, const char *v, int vl, int op);
int db_del(isdb_t *, const char *k, int kl);

#endif
