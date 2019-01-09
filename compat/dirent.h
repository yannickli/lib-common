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

#ifndef IS_COMPAT_DIRENT_H
#define IS_COMPAT_DIRENT_H

#include_next <dirent.h>

#ifndef __GLIBC__
#define __is_need_dirfd
int dirfd(DIR *dir);

#endif

#endif /* !IS_COMPAT_DIRENT_H */

