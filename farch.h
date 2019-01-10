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

typedef struct farch_entry_t {
    const char *name;
    const void *data;
    int size;
} farch_entry_t;

static inline
const farch_entry_t *farch_get(const farch_entry_t files[], const char *name)
{
    for (; files->name; files++) {
        if (strequal(files->name, name)) {
            return files;
        }
    }
    return NULL;
}

static inline pstream_t ps_initfarch(const farch_entry_t *fe)
{
    return ps_init(fe->data, fe->size);
}

#endif /* IS_LIB_COMMON_FARCH_H */
