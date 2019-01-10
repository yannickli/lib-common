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

#ifndef IS_LIB_COMMON_FARCH_H
#define IS_LIB_COMMON_FARCH_H

#include "core.h"

typedef struct farch_t farch_t;
typedef struct farch_entry_t {
    const char *name;
    const void *data;
    int size;
} farch_entry_t;

farch_t *farch_new(const farch_entry_t files[], const char *overridedir);
void farch_add(farch_t *fa, const farch_entry_t files[]);
void farch_delete(farch_t **fa);

const farch_entry_t *farch_find(const farch_t *fa, const char *name);

int farch_get(farch_t *fa, sb_t *buf, const byte **data, int *size,
              const char *name);

#endif /* IS_LIB_COMMON_FARCH_H */
