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

#include "file-log.h"

/* log file names should depend on rotation scheme: slower rotation
 * scheme should shorten log filename so reopening it yields the same
 * file
 */
static int build_real_path(char *buf, int size, log_file_t *log_file, time_t date)
{
    struct tm tm;
    localtime_r(&date, &tm);
    return snprintf(buf, size, "%s_%04d%02d%02d_%02d%02d%02d.%s",
                    log_file->prefix, tm.tm_year + 1900, tm.tm_mon + 1,
                    tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                    log_file->ext);
}

static void log_check_max_files(log_file_t *log_file)
{
    if (log_file->max_files > 0) {
        glob_t globbuf;
        char buf[PATH_MAX];
        int dl;

        snprintf(buf, sizeof(buf), "%s_????????_??????.%s",
                 log_file->prefix, log_file->ext);
        if (glob(buf, 0, NULL, &globbuf)) {
            globfree(&globbuf);
            return;
        }
        dl = (int)globbuf.gl_pathc - log_file->max_files;
        for (int i = 0; i < dl; i++) {
            unlink(globbuf.gl_pathv[i]);
        }
        globfree(&globbuf);
    }
}

static file_t *log_file_open_new(log_file_t *log_file, time_t date)
{
    char real_path[PATH_MAX];
    char sym_path[PATH_MAX];
    file_t *res;

    build_real_path(real_path, sizeof(real_path), log_file, date);
    res = file_open(real_path, FILE_WRONLY | FILE_CREATE, 0644);
    if (!res || file_seek(res, 0, SEEK_END) == (off_t)-1) {
        e_trace(1, "Could not open log file: %s (%m)", real_path);
    }

    /* Add a symlink */
    snprintf(sym_path, sizeof(sym_path), "%s%s.%s", log_file->prefix,
             log_file->use_last ? "_last" : "", log_file->ext);
    unlink(sym_path);
    if (symlink(real_path, sym_path)) {
        e_trace(1, "Could not symlink %s to %s (%m)", real_path, sym_path);
    }
    log_check_max_files(log_file);
    return res;
}

static time_t log_last_date(log_file_t *log_file)
{
    char buf[PATH_MAX];
    glob_t globbuf;
    time_t res = -1;

    snprintf(buf, sizeof(buf), "%s_????????_??????.%s",
             log_file->prefix, log_file->ext);
    if (!glob(buf, 0, NULL, &globbuf) && globbuf.gl_pathc) {
        const byte *d = (const byte *)globbuf.gl_pathv[globbuf.gl_pathc - 1];
        struct tm tm;

        d += strlen(log_file->prefix) + 1;
        /* %04d%02d%02d_%02d%02d%02d */
        p_clear(&tm, 1);
        tm.tm_isdst = -1; /* We don't know current dst */
        tm.tm_year = memtoip(d, 4, &d) - 1900;
        tm.tm_mon  = memtoip(d, 2, &d) - 1;
        tm.tm_mday = memtoip(d, 2, &d);
        if (*d++ != '_')
            return -1;
        tm.tm_hour = memtoip(d, 2, &d);
        tm.tm_min  = memtoip(d, 2, &d);
        tm.tm_sec  = memtoip(d, 2, &d);
        res = mktime(&tm);
    }
    globfree(&globbuf);
    return res;
}

log_file_t *log_file_open(const char *nametpl, int flags)
{
    log_file_t *log_file;
    const char *ext = path_ext(nametpl);
    int len = strlen(nametpl);

    log_file = p_new(log_file_t, 1);
    if (flags & LOG_FILE_USE_LAST)
        log_file->use_last = true;

    if (len + 8 + 1 + 6 + 4 >= ssizeof(log_file->prefix)) {
        e_trace(1, "Path format too long");
        IGNORE(log_file_close(&log_file));
        return NULL;
    }
    if (ext) {
        pstrcpy(log_file->ext, sizeof(log_file->ext), ext + 1);
        len -= strlen(ext);
    } else {
        pstrcpy(log_file->ext, sizeof(log_file->ext), "log");
    }
    pstrcpylen(log_file->prefix, sizeof(log_file->prefix), nametpl, len);
    log_file->open_date = log_last_date(log_file);
    if (log_file->open_date == -1) {
        log_file->open_date = time(NULL);
    }
    log_file->_internal = log_file_open_new(log_file, log_file->open_date);
    if (!log_file->_internal) {
        e_trace(1, "Could not open first log file");
        IGNORE(log_file_close(&log_file));
    }
    return log_file;
}

int log_file_close(log_file_t **lfp)
{
    int res = 0;
    if (*lfp) {
        log_file_t *log_file = *lfp;
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

void log_file_set_rotate_delay(log_file_t *file, time_t delay)
{
    file->rotate_delay = delay;
    file->rotate_date = file->open_date + file->rotate_delay;
}

static int log_file_rotate_(log_file_t *file, time_t now)
{
    IGNORE(file_close(&file->_internal));
    file->_internal = log_file_open_new(file, now);
    file->open_date = now;
    if (!file->_internal) {
        e_trace(1, "Could not rotate");
        return -1;
    }
    if (file->rotate_delay > 0)
        file->rotate_date = now + file->rotate_delay;
    return 0;
}

int log_file_rotate(log_file_t *lf)
{
    return log_file_rotate_(lf, time(NULL));
}

static int log_check_rotate(log_file_t *log_file)
{
    if (log_file->max_size > 0) {
        off_t size = file_tell(log_file->_internal);
        if (size >= log_file->max_size)
             return log_file_rotate_(log_file, time(NULL));
    }
    if (log_file->rotate_delay > 0) {
        time_t now = time(NULL);
        if (log_file->rotate_date <= now)
            return log_file_rotate_(log_file, now);
    }
    return 0;
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

int log_file_flush(log_file_t *log_file)
{
    if (log_file->_internal)
        return file_flush(log_file->_internal);
    return 0;
}
