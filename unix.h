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
void ps_panic_sighandler(int signum, siginfo_t *si, void *addr);
void ps_install_panic_sighandlers(void);

/****************************************************************************/
/* Filesystem related                                                       */
/****************************************************************************/

/* XXX man 2 open
 * NOTES: alignment boundaries on linux 2.6 for direct I/O is 512 bytes
 */
#define DIRECT_BITS            9
#define DIRECT_ALIGN           (1 << DIRECT_BITS)
#define DIRECT_REMAIN(Val)     ((Val) & BITMASK_LT(typeof(Val), DIRECT_BITS))
#define DIRECT_TRUNCATE(Val)   ((Val) & BITMASK_GE(typeof(Val), DIRECT_BITS))
#define DIRECT_IS_ALIGNED(Val) (DIRECT_REMAIN(Val) == 0)

int mkdir_p(const char *dir, mode_t mode);
int rmdir_r(const char *dir, bool only_content);

int get_mtime(const char *filename, time_t *t);

off_t filecopy(const char *pathin, const char *pathout);
off_t filecopyat(int dfd_src, const char* name_src,
                 int dfd_dst, const char* name_dst);

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
__must_check__ int xpwrite(int fd, const void *data, ssize_t dlen, off_t offset);
__must_check__ int xftruncate(int fd, off_t offs);
__must_check__ int xread(int fd, void *data, ssize_t dlen);
__must_check__ int xpread(int fd, void *data, ssize_t dlen, off_t offset);
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

/** \brief Get PID of a traced process
 *
 * \param pid  PID of the process (0 = current process)
 *
 * \return  the PID of the tracer
 * \return  0 if the process is not traced
 * \return -1 on error
 */
pid_t psinfo_get_tracer_pid(pid_t pid);

/* XXX: This function MUST be inlined in check_strace() to avoid appearing
 * in the stack.
 */
static ALWAYS_INLINE int _psinfo_get_tracer_pid(pid_t pid)
{
    t_scope;
    sb_t      buf;
    pstream_t ps;
    pid_t     tpid;

    t_sb_init(&buf, (2 << 10));

    if (pid <= 0) {
        RETHROW(sb_read_file(&buf, "/proc/self/status"));
    } else {
        char path[PATH_MAX];

        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        RETHROW(sb_read_file(&buf, path));
    }

    ps = ps_initsb(&buf);

    while (!ps_done(&ps)) {
        if (ps_skipstr(&ps, "TracerPid:") >= 0) {
            tpid = ps_geti(&ps);
            return tpid > 0 ? tpid : 0;
        }
        RETHROW(ps_skip_afterchr(&ps, '\n'));
    }

    return -1;
}

#endif /* IS_LIB_COMMON_UNIX_H */
