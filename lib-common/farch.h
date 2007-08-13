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

#ifndef IS_LIB_COMMON_FARCH_H
#define IS_LIB_COMMON_FARCH_H

#include <lib-common/blob.h>
#include "macros.h"

typedef struct farch farch;

typedef struct farch_file {
    int namehash;
    const char *name;
    int size;
    const byte *data;
} farch_file;

/* Public interface */
farch *farch_generic_new(const char *overridedir);
void farch_generic_delete(farch **fa);
int farch_generic_get(const farch_file files[], const char *name,
                      const byte **data, int *size);
int farch_generic_get_withvars(blob_t *out, const farch_file files[],
                               const char *name, int nbvars,
                               const char **vars, const char **values);

/* For internal use only */
int farch_namehash(const char *name);
#endif /* IS_LIB_COMMON_FARCH_H */
