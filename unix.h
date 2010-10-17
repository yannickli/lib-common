/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_UNIX_H
#define IS_LIB_COMMON_UNIX_H

#include "container.h"

#ifndef O_CLOEXEC
# ifdef OS_LINUX
#  define O_CLOEXEC	02000000	/* set close_on_exec */
# else
#  error "set O_CLOEXEC for your platform here"
# endif
#endif

#define O_ISWRITE(m)      (((m) & (O_RDONLY|O_WRONLY|O_RDWR)) != O_RDONLY)

#define PROTECT_ERRNO(expr) \
    ({ int save_errno__ = errno; expr; errno = save_errno__; })

#define ERR_RW_RETRIABLE(err) \
    ((err) == EINTR || (err) == EAGAIN)
#define ERR_CONNECT_RETRIABLE(err) \
    ((err) == EINTR || (err) == EINPROGRESS)
#define ERR_ACCEPT_RETRIABLE(err) \
    ((err) == EINTR || (err) == EAGAIN || (err) == ECONNABORTED)

/****************************************************************************/
/* process related                                                          */
/****************************************************************************/

int  pid_get_starttime(pid_t pid, struct timeval *tv);
void ps_dump_backtrace(int signum, const char *prog, int fd, bool full);
void ps_panic_sighandler(int signum);
void ps_install_panic_sighandlers(void);

/****************************************************************************/
/* Filesystem related                                                       */
/****************************************************************************/

int mkdir_p(const char *dir, mode_t mode);
int rmdir_r(const char *dir, bool only_content);

int get_mtime(const char *filename, time_t *t);

int filecopy(const char *pathin, const char *pathout);

int p_lockf(int fd, int mode, int cmd, off_t start, off_t len);
int p_unlockf(int fd, off_t start, off_t len);

int tmpfd(void);
void devnull_dup(int fd);

/****************************************************************************/
/* file descriptor related                                                  */
/****************************************************************************/

/* `data' is cast to `void *' to remove the const: iovec structs are used
 * for input and output, thus `iov_base' cannot be `const void *' even
 * for write operations.
 */
#define MAKE_IOVEC(data, len)  \
     (struct iovec){ .iov_base = (void *)(data), .iov_len = (len) }

qvector_t(iovec, struct iovec);

static inline size_t iovec_len(const struct iovec *iov, int iovlen)
{
    size_t res = 0;
    for (int i = 0; i < iovlen; i++) {
        res += iov[i].iov_len;
    }
    return res;
}

int iovec_vector_kill_first(qv_t(iovec) *iovs, ssize_t len);


__must_check__ int xwrite_file(const char *file, const void *data, ssize_t dlen);
__must_check__ int xwrite(int fd, const void *data, ssize_t dlen);
__must_check__ int xwritev(int fd, struct iovec *iov, int iovcnt);
__must_check__ int xftruncate(int fd, off_t offs);
__must_check__ int xread(int fd, void *data, ssize_t dlen);
bool is_fd_open(int fd);
/* FIXME: Find a better name. */
int close_fds_higher_than(int fd);
bool is_fancy_fd(int fd);
void term_get_size(int *cols, int *rows);

int fd_set_features(int fd, int flags);
int fd_unset_features(int fd, int flags);

__attr_nonnull__((1))
static inline int p_fclose(FILE **fpp) {
    FILE *fp = *fpp;

    *fpp = NULL;
    return fp ? fclose(fp) : 0;
}

__attr_nonnull__((1))
static inline int p_close(int *hdp) {
    int hd = *hdp;
    *hdp = -1;
    if (hd < 0)
        return 0;
    while (close(hd) < 0) {
        if (!ERR_RW_RETRIABLE(errno))
            return -1;
    }
    return 0;
}

/****************************************************************************/
/* Misc                                                                     */
/****************************************************************************/

static inline void getopt_init(void) {
    /* XXX this is not portable, BSD want it to be set to -1 *g* */
    optind = 0;
}

/* if pid <= 0, retrieve infos for the current process */
int psinfo_get(pid_t pid, sb_t *output);

#endif /* IS_LIB_COMMON_UNIX_H */
