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

#ifndef IS_LIB_COMMON_UNIX_H
#define IS_LIB_COMMON_UNIX_H

#include "core.h"

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

int pid_get_starttime(pid_t pid, struct timeval *tv);

/****************************************************************************/
/* Filesystem related                                                       */
/****************************************************************************/

int mkdir_p(const char *dir, mode_t mode);
int rmdir_r(const char *dir, bool only_content);

int get_mtime(const char *filename, time_t *t);

int filecopy(const char *pathin, const char *pathout);

int p_lockf(int fd, int mode, int cmd, off_t start, off_t len);
int p_unlockf(int fd, off_t start, off_t len);

/** Directory lock */
typedef struct dir_lock_t {
    int dfd;    /**< Directory file descriptor */
    int lockfd; /**< Lock file descriptor      */
} dir_lock_t;

/** Directory lock initializer
 *
 * Since 0 is a valid file descriptor, dir_lock_t fds must be initialiazed to
 * -1.
 */
#define DIR_LOCK_INIT    { .dfd = -1, .lockfd = -1 }
#define DIR_LOCK_INIT_V  (dir_lock_t)DIR_LOCK_INIT

/** Lock a directory
 * \param  dfd    directory file descriptor
 * \param  dlock  directory lock
 * \retval  0 on success
 * \retval -1 on error
 * \see    unlockdir
 *
 * Try to create a .lock file (with u+rw,g+r,o+r permissions) into the given
 * directory and lock it.
 *
 * An error is returned if the directory is already locked (.lock exists and
 * is locked) or if the directory cannot be written. In this case, errno is
 * set appropriately.
 *
 * After a sucessful call to lockdir(), the directory file descriptor (dfd) is
 * duplicated for internal usage. The file descriptors of the dir_lock_t
 * should not be used by the application.
 *
 * Use unlockdir() to unlock a directory locked with unlock().
 */
int lockdir(int dfd, dir_lock_t *dlock);

/** Unlock a directory
 * \param  dlock  directory lock
 * \see    lockdir
 *
 * Unlock the .lock file and delete it. unlockdir() be called on a file
 * descriptor returned by lockdir().
 *
 * To be safe, this function resets the file descriptors to -1.
 */
void unlockdir(dir_lock_t *dlock);

int tmpfd(void);
void devnull_dup(int fd);

/****************************************************************************/
/* file descriptor related                                                  */
/****************************************************************************/

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
