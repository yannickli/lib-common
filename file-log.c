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

#include <dirent.h>
#include <glob.h>
#include <sys/resource.h>

#include "file-log.h"
#include "el.h"
#include "unix.h"
#include "thr.h"

/* log file names should depend on rotation scheme: slower rotation
 * scheme should shorten log filename so reopening it yields the same
 * file
 */
static const char *t_build_real_path(int *size, log_file_t *log_file,
                                     time_t date)
{
    struct tm tm;

    if (log_file->flags & LOG_FILE_UTCSTAMP) {
        gmtime_r(&date, &tm);
    } else {
        localtime_r(&date, &tm);
    }
    return t_fmt(size, "%s_" LOG_FILE_DATE_FMT ".%s",
                 log_file->prefix, LOG_FILE_DATE_FMT_ARG(tm), log_file->ext);
}

static void
log_file_bgcompress_check(el_t ev, pid_t pid, int st, data_t priv)
{
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        e_error("background compression of log file failed");
    }
}

static void log_file_bgcompress(const char *path)
{
    sigset_t set;
    pid_t pid;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    pid = fork();
    if (pid < 0) {
        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
        e_error("unable to fork gzip in the background, %m");
        return;
    }
    if (pid > 0) {
        el_unref(el_child_register(pid, log_file_bgcompress_check, NULL));
        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    } else {
        setsid();
        setpriority(PRIO_PROCESS, getpid(), NZERO / 4);
        execlp("gzip", "gzip", "-9", path, NULL);
        e_error("execl failed: %m");
        _exit(0);
    }
}

static int qsort_strcmp(const void *sp1, const void *sp2)
{
    return strcmp(*(const char **)sp1, *(const char **)sp2);
}

static void log_file_call_cb(log_file_t *l, enum log_file_event evt,
                             const char *fname)
{
    if (!l->on_event)
        return;

    {
        t_scope;

        if (!fname) {
            fname = t_build_real_path(NULL, l, l->open_date);
        }

        (*l->on_event)(l, evt, fname, l->priv_cb);
    }
}

static void log_delete_file(log_file_t *log_file, const char *path)
{
    if (unlink(path) < 0) {
        e_error("cannot delete log file '%s': %m", path);
    } else {
        log_file_call_cb(log_file, LOG_FILE_DELETE, path);
    }
}

static void log_check_invariants(log_file_t *log_file)
{
    glob_t globbuf;
    char buf[PATH_MAX];
    char **fv;
    int  fc;

    snprintf(buf, sizeof(buf), "%s_????????_??????.%s{,.gz}",
             log_file->prefix, log_file->ext);
    if (glob(buf, GLOB_BRACE, NULL, &globbuf)) {
        globfree(&globbuf);
        return;
    }
    fv = globbuf.gl_pathv;
    fc = globbuf.gl_pathc;
    qsort(fv, fc, sizeof(fv[0]), qsort_strcmp);
    if (log_file->max_files) {
        for (; fc > log_file->max_files; fc--, fv++) {
            if (log_file->flags & LOG_FILE_COMPRESS) {
                if (strequal(path_extnul(fv[0]), ".gz")) {
                    log_delete_file(log_file, fv[0]);
                } else {
                    break;
                }
            } else {
                log_delete_file(log_file, fv[0]);
            }
        }
    }
    if (log_file->max_total_size) {
        int64_t totalsize = (int64_t)log_file->max_total_size << 20;
        struct stat st;

        for (int i = fc; i-- > 0; ) {
            if ((log_file->flags & LOG_FILE_COMPRESS)
            &&  !strequal(path_extnul(fv[i]), ".gz"))
            {
                /* XXX: uncompressed files must be compressed so skip it
                 * while accounting totalsize */
                continue;
            }
            if (lstat(fv[i], &st) == 0 && S_ISREG(st.st_mode))
                totalsize -= st.st_size;
            if (totalsize < 0) {
                for (int j = 0; j <= i; j++) {
                    log_delete_file(log_file, fv[j]);
                }
                fv += i + 1;
                fc -= i + 1;
                break;
            }
        }
    }
    if (log_file->flags & LOG_FILE_COMPRESS) {
        for (int i = 0; i < fc - 1; i++) {
            if (!strequal(path_extnul(fv[i]), ".gz")) {
                log_file_bgcompress(fv[i]);
            }
        }
    }
    globfree(&globbuf);
}

static void log_file_open_new(log_file_t *log_file, time_t date)
{
    t_scope;
    const char *real_path;

    real_path = t_build_real_path(NULL, log_file, date);

    log_file->open_date = date;
    log_file->_internal = file_open(real_path, FILE_WRONLY | FILE_CREATE |
                                    FILE_EXCL, 0644);
    if (log_file->_internal) {
        log_file_call_cb(log_file, LOG_FILE_CREATE, NULL);
    } else {
        if (errno == EEXIST) {
            log_file->_internal = file_open(real_path, FILE_WRONLY, 0644);
        }
        if (!log_file->_internal
        ||  file_seek(log_file->_internal, 0, SEEK_END) == (off_t)-1)
        {
            e_trace(1, "could not open log file: %s (%m)", real_path);
        }
    }

    if (!(log_file->flags & LOG_FILE_NOSYMLINK)) {
        char sym_path[PATH_MAX];

        snprintf(sym_path, sizeof(sym_path), "%s%s.%s", log_file->prefix,
                 log_file->flags & LOG_FILE_USE_LAST ? "_last" : "", log_file->ext);
        unlink(sym_path);
        if (symlink(real_path, sym_path)) {
            e_trace(1, "could not symlink %s to %s (%m)", real_path, sym_path);
        }
    }
    log_check_invariants(log_file);
}

static void log_file_find_last_date(log_file_t *log_file)
{
    char buf[PATH_MAX];
    glob_t globbuf;

    log_file->open_date = time(NULL);

    snprintf(buf, sizeof(buf), "%s_????????_??????.%s",
             log_file->prefix, log_file->ext);
    if (!glob(buf, 0, NULL, &globbuf) && globbuf.gl_pathc) {
        log_file_get_file_stamp(log_file,
                                globbuf.gl_pathv[globbuf.gl_pathc - 1],
                                &log_file->open_date);
    }
    globfree(&globbuf);
}

log_file_t *log_file_init(log_file_t *log_file, const char *nametpl, int flags)
{
    const char *ext = path_ext(nametpl);
    int len = strlen(nametpl);

    p_clear(log_file, 1);
    log_file->flags = flags;

    if (len + 8 + 1 + 6 + 4 >= ssizeof(log_file->prefix))
        e_fatal("path format too long");
    if (ext) {
        pstrcpy(log_file->ext, sizeof(log_file->ext), ext + 1);
        len -= strlen(ext);
    } else {
        pstrcpy(log_file->ext, sizeof(log_file->ext), "log");
    }
    pstrcpymem(log_file->prefix, sizeof(log_file->prefix), nametpl, len);
    return log_file;
}

log_file_t *log_file_new(const char *nametpl, int flags)
{
    return log_file_init(p_new_raw(log_file_t, 1), nametpl, flags);
}

log_file_t *
log_file_create_from_iop(const char *nametpl,
                         const core__log_file_configuration__t *conf,
                         int flags, log_file_cb_f *on_event, void *priv)
{
    log_file_t *log_file;

    if (conf->compress) {
        flags |= LOG_FILE_COMPRESS;
    }

    log_file = log_file_new(nametpl, flags);

    log_file_set_maxtotalsize(log_file, conf->total_max_size / (1024 * 1024));
    log_file_set_maxfiles(log_file, conf->max_files);
    log_file_set_maxsize(log_file, conf->max_size);
    log_file_set_rotate_delay(log_file, conf->max_time);

    if (on_event) {
        log_file_set_file_cb(log_file, on_event, priv);
    }
    if (log_file_open(log_file)) {
        e_error("cannot open log file %s: %m", nametpl);
        PROTECT_ERRNO(IGNORE(log_file_close(&log_file)));
        return NULL;
    }

    if (e_name_is_traced(1, "file-log")) {
        SB_1k(sb);

        core__log_file_configuration__sb_jpack(&sb, conf, IOP_JPACK_COMPACT);
        e_named_trace(1, "file-log", "opening %s, flags %x, json %*pM",
                      nametpl, flags, SB_FMT_ARG(&sb));
    }

    return log_file;
}

int log_file_open(log_file_t *log_file)
{
    log_file_find_last_date(log_file);
    log_file_open_new(log_file, log_file->open_date);
    if (!log_file->_internal) {
        e_trace(1, "could not open first log file");
        return -1;
    }
    return 0;
}

int log_file_close(log_file_t **lfp)
{
    int res = 0;
    if (*lfp) {
        log_file_t *log_file = *lfp;

        log_file_call_cb(log_file, LOG_FILE_CLOSE, NULL);
        log_file_flush(log_file);
        res = file_close(&log_file->_internal);
        p_delete(lfp);
    }
    return res;
}

void log_file_set_maxsize(log_file_t *file, int max)
{
    if (max < 0) {
        max = 0;
    }
    file->max_size = max;
}

void log_file_set_maxfiles(log_file_t *file, int maxfiles)
{
    if (maxfiles < 0) {
        maxfiles = 0;
    }
    file->max_files = maxfiles;
}

void log_file_set_maxtotalsize(log_file_t *file, int maxtotalsize)
{
    file->max_total_size = MAX(0, maxtotalsize);
}

void log_file_set_rotate_delay(log_file_t *file, time_t delay)
{
    file->rotate_delay = delay;
}

void
log_file_set_file_cb(log_file_t *file,
                     log_file_cb_f *on_event,
                     void *priv)
{
    file->on_event = on_event;
    file->priv_cb = priv;
}

static int log_file_rotate_(log_file_t *file, time_t now)
{
    if (file->open_date == now)
        return 0;

    RETHROW(file_close(&file->_internal));

    log_file_open_new(file, now);
    if (!file->_internal) {
        e_trace(1, "could not rotate");
        return -1;
    }

    return 0;
}

static int log_check_rotate(log_file_t *lf)
{
    if (lf->max_size > 0) {
        off_t size = file_tell(lf->_internal);
        if (size >= lf->max_size)
             return log_file_rotate_(lf, time(NULL));
    }
    if (lf->rotate_delay > 0) {
        time_t now = time(NULL);
        if (lf->open_date + lf->rotate_delay <= now)
            return log_file_rotate_(lf, now);
    }
    return 0;
}

int log_file_rotate(log_file_t *file)
{
    return log_file_rotate_(file, time(NULL));
}

int log_fprintf(log_file_t *log_file, const char *format, ...)
{
    int res;
    va_list ap;

    if (log_check_rotate(log_file))
        return -1;
    va_start(ap, format);
    res = file_writevf(log_file->_internal, format, ap);
    va_end(ap);
    return res;
}

int log_fwrite(log_file_t *log_file, const void *data, size_t len)
{
    if (log_check_rotate(log_file))
        return -1;
    return file_write(log_file->_internal, data, len);
}

int log_fwritev(log_file_t *log_file, struct iovec *iov, size_t iovlen)
{
    if (log_check_rotate(log_file))
        return -1;
    return file_writev(log_file->_internal, iov, iovlen);
}

int log_file_flush(log_file_t *log_file)
{
    if (log_file->_internal)
        return file_flush(log_file->_internal);
    return 0;
}

int log_file_get_file_stamp(const log_file_t *file, const char *path,
                            time_t *out)
{
    struct tm tm;
    const byte *d;
    int prefix_len = strlen(file->prefix);


    if (strlen(path) < prefix_len + strlen("YYYYMMDD_hhmmss")) {
        return -1;
    }
    d = (const byte *)path + prefix_len + 1;

    p_clear(&tm, 1);
    tm.tm_isdst = -1; /* We don't know current dst */
    tm.tm_year = memtoip(d, 4, &d) - 1900;
    tm.tm_mon  = memtoip(d, 2, &d) - 1;
    tm.tm_mday = memtoip(d, 2, &d);
    if (*d++ != '_') {
        return -1;
    }
    tm.tm_hour = memtoip(d, 2, &d);
    tm.tm_min  = memtoip(d, 2, &d);
    tm.tm_sec  = memtoip(d, 2, &d);
    if (file->flags & LOG_FILE_UTCSTAMP) {
        *out = timegm(&tm);
    } else {
        *out = mktime(&tm);
    }

    return 0;
}
