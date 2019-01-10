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

#ifndef IS_COMPAT_FCNTL_H
#define IS_COMPAT_FCNTL_H

#include <sys/types.h>

#include_next <fcntl.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef __GLIBC__
#define __is_need_posix_fallocate
int posix_fallocate(int fd, off_t offset, off_t len);

#endif

#ifndef __USE_ATFILE
# ifndef AT_SYMLINK_NOFOLLOW
#  define AT_SYMLINK_NOFOLLOW  0x100  /* Do not follow symbolic links. */
# endif
#endif

#endif /* !IS_COMPAT_FCNTL_H */

