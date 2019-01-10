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

/* XXX: Do not move in the guard, this will fail unexpectedly... */
#include_next <stdlib.h>

#ifndef IS_COMPAT_STDLIB_H
#define IS_COMPAT_STDLIB_H

#ifndef __GLIBC__
#define __is_need_posix_fallocate
int posix_fallocate(int fd, off_t offset, off_t len);

#endif

#endif /* !IS_COMPAT_STDLIB_H */

