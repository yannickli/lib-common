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

#ifndef IS_LIB_COMMON_FARCH_H
#define IS_LIB_COMMON_FARCH_H

#include "hashtbl.h"
#include "blob.h"

typedef struct farch_t farch_t;
typedef struct farch_entry_t {
    const char *name;
    const byte *data;
    int size;
} farch_entry_t;

farch_t *farch_new(const farch_entry_t files[], const char *overridedir);
void farch_delete(farch_t **fa);

int farch_get(const farch_t *fa, blob_t *buf, const byte **data, int *size,
              const char *name);

#endif /* IS_LIB_COMMON_FARCH_H */
