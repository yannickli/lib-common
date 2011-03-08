/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_COMPAT_SYS_STAT_H
#define IS_COMPAT_SYS_STAT_H

#include_next <sys/stat.h>

#ifndef __USE_ATFILE
# define __is_need_at_replacement
int fstatat(int dirfd, const char *pathname, struct stat *buf,
            int flags);
#endif

#endif /* !IS_COMPAT_SYS_STAT_H */
