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

#ifndef IS_COMPAT_SYS_STAT_H
#define IS_COMPAT_SYS_STAT_H

#include_next <sys/stat.h>

#ifndef __USE_ATFILE
# define __is_need_at_replacement
int fstatat(int dirfd, const char *pathname, struct stat *buf,
            int flags);
#endif

#ifdef __APPLE__
#define st_atimensec  st_atimespec.tv_nsec
#define st_mtimensec  st_mtimespec.tv_nsec
#define st_ctimensec  st_ctimespec.tv_nsec
#endif

#endif /* !IS_COMPAT_SYS_STAT_H */
