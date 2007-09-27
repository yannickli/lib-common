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

#ifndef IS_LIB_COMMON_PSINFO_H
#define IS_LIB_COMMON_PSINFO_H

#include <sys/types.h>

#include <lib-common/blob.h>

/* if pid <= 0, retrieve infos for the current process */
int psinfo_get(pid_t pid, blob_t *output);

#endif /* IS_LIB_COMMON_PSINFO_H */
