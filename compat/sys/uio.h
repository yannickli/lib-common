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

#ifndef IS_COMPAT_SYS_UIO_H
#define IS_COMPAT_SYS_UIO_H

#if defined(__MINGW) || defined(__MINGW32__)
#  include_next <unistd.h>
struct iovec {
    void *iov_base;   /* Starting address */
    size_t iov_len;   /* Number of bytes */
};
extern ssize_t readv(int __fd, __const struct iovec *__iovec, int __count);
extern ssize_t writev(int __fd, __const struct iovec *__iovec, int __count);
#else
#  include_next <sys/uio.h>
#endif

#endif /* !IS_COMPAT_SYS_UIO_H */

