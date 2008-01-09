/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_ISNDX_H
#define IS_LIB_COMMON_ISNDX_H

#include "macros.h"
#include "blob.h"
#include "mmappedfile.h"

typedef struct isndx_t isndx_t;

typedef struct isndx_create_parms_t {
    int flags;
    int pageshift;
    int minkeylen, maxkeylen;
    int mindatalen, maxdatalen;
} isndx_create_parms_t;

isndx_t *isndx_create(const char *path, isndx_create_parms_t *cp);

isndx_t *isndx_open(const char *path, int flags);
void isndx_close(isndx_t **ndx);

int isndx_fetch(isndx_t *ndx, const byte *key, int klen, blob_t *out);
int isndx_push(isndx_t *ndx, const byte *key, int klen,
               const void *data, int len);

int isndx_fetch_uint64(isndx_t *ndx, uint64_t key, blob_t *out);
int isndx_push_uint64(isndx_t *ndx, uint64_t key, const void *data, int len);

#define ISNDX_CHECK_ALL          1
#define ISNDX_CHECK_PAGES        2
#define ISNDX_CHECK_KEYS         4
#define ISNDX_CHECK_ISRIGHTMOST  8

int isndx_check(isndx_t *ndx, int flags);
int isndx_check_page(isndx_t *ndx, uint32_t pageno, int level, int flags);

#define ISNDX_DUMP_ALL      1
#define ISNDX_DUMP_PAGES    2
#define ISNDX_DUMP_KEYS     4

void isndx_dump(isndx_t *ndx, int flags, FILE *fp);
void isndx_dump_page(isndx_t *ndx, uint32_t pageno, int flags, FILE *fp);

int isndx_get_error(isndx_t *ndx, char *buf, int size);
FILE *isndx_set_error_stream(isndx_t *ndx, FILE *stream);

#endif /* IS_LIB_COMMON_ISNDX_H */
