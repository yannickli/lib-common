/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIBCOMMON_COMPAT_DLFCN_H
#define IS_LIBCOMMON_COMPAT_DLFCN_H

#include_next <dlfcn.h>

#ifdef __APPLE__
typedef enum {
    LM_ID_BASE,
    LM_ID_NEWLM,
} Lmid_t;

static inline void *dlmopen(Lmid_t lmid, const char *filename, int flags)
{
    if (lmid == LM_ID_BASE) {
        flags |= RTLD_GLOBAL;
    } else {
        flags |= RTLD_LOCAL;
    }
    return dlopen(filename, flags);
}

typedef enum {
    RTLD_DI_LMID,
} Linfo_t;

static inline int dlinfo(void *handle, Linfo_t request, void *info)
{
    if (request == RTLD_DI_LMID) {
        *(Lmid_t *)info = LM_ID_BASE;
    }
    return 0;
}
#endif

#endif
