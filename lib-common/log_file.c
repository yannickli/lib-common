/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef MINGCC
#include <sys/stat.h>
#include <dirent.h>
#include <glob.h>

#include "unix.h"
#include "blob.h"
#include "string_is.h"
#include "log_file.h"

GENERIC_INIT(log_file_t, log_file);
static void log_file_wipe(log_file_t *log_file)
{
    log_file_flush(log_file);
    p_fclose(&log_file->_internal);
    p_clear(log_file, 1);
}
GENERIC_NEW(log_file_t, log_file);
GENERIC_DELETE(log_file_t, log_file);

/* log file names should depend on rotation scheme: slower rotation
 * scheme should shorten log filename so reopening it yields the same
 * file
 */
static int build_real_path(char *buf, int size, log_file_t *logfile, time_t date)
{
    struct tm *cur_date;

    cur_date = localtime(&date);
    return snprintf(buf, size, "%s_%04d%02d%02d_%02d%02d%02d.%s",
                    logfile->prefix,
                    cur_date->tm_year + 1900, cur_date->tm_mon + 1,
                    cur_date->tm_mday, cur_date->tm_hour,
                    cur_date->tm_min, cur_date->tm_sec,
                    logfile->ext);
}


static FILE *log_file_open_new(log_file_t *logfile, time_t date)
{
    char real_path[PATH_MAX];
    char sym_path[PATH_MAX];
    int len;
    FILE *res;

    len = build_real_path(real_path, sizeof(real_path), logfile, date);
    assert (len < ssizeof(real_path));
    res = fopen(real_path, "a");
    if (!res) {
        e_trace(1, "Could not open log file: %s (%m)", real_path);
    }

    /* Add a symlink */
    len = snprintf(sym_path, sizeof(sym_path), "%s_last.%s",
                   logfile->prefix, logfile->ext);
    if (len >= ssizeof(sym_path)) {
        e_trace(1, "Sym path too long");
    } else {
        unlink(sym_path);
        if (symlink(real_path, sym_path)) {
            e_trace(1, "Could not symlink %s to %s (%m)",
                    real_path, sym_path);
        }
    }

    return res;
}

static int log_last_date(const char *prefix, const char *ext)
{
    char path[PATH_MAX];
    const char *p;
    char sympath[PATH_MAX];
    ssize_t len;
    struct tm cur_date;

    /* Try to find the last log file */
    len = snprintf(path, sizeof(path), "%s_last.%s", prefix, ext);
    assert (len < ssizeof(path));
    len = readlink(path, sympath, sizeof(sympath));
    if (len < 0) {
        if (errno != ENOENT) {
            e_trace(1, "Could not readlink %s: %m", path);
        }
        return 0;
    }

    if (len >= ssizeof(sympath)) {
        e_trace(1, "Linked path is too long");
        return 0;
    }
    sympath[len] = '\0';
    /* Read %04d%02d%02d_%02d%02d%02d.log suffix */
    if (!strstart(sympath, prefix, &p) || *p != '_') {
        /* Bad prefix... */
        return 0;
    }
    p++;
    len = sscanf(p, "%4d%2d%2d_%2d%2d%2d.",
                 &cur_date.tm_year, &cur_date.tm_mon,
                 &cur_date.tm_mday, &cur_date.tm_hour,
                 &cur_date.tm_min, &cur_date.tm_sec);
    if (len != 6) {
        return 0;
    }

    if (cur_date.tm_year < 1900 || cur_date.tm_mon < 1) {
        e_trace(1, "Bad timestamp: %s", p);
        return 0;
    }

    cur_date.tm_year -= 1900;
    cur_date.tm_mon -= 1;

    e_trace(2, "Found last log file : %s", p);
    return mktime(&cur_date);
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

log_file_t *log_file_open(const char *nametpl)
{
    log_file_t *log_file;
    const char *ext = get_ext(nametpl);
    int len = strlen(nametpl);

    log_file = log_file_new();

    if (len + 8 + 1 + 6 + 4 >= ssizeof(log_file->prefix)) {
        e_trace(1, "Path format too long");
        log_file_delete(&log_file);
        return NULL;
    }
    if (ext) {
        pstrcpy(log_file->ext, sizeof(log_file->ext), ext + 1);
        len -= strlen(ext);
    } else {
        pstrcpy(log_file->ext, sizeof(log_file->ext), "log");
    }
    pstrcpylen(log_file->prefix, sizeof(log_file->prefix), nametpl, len);
    log_file->open_date = log_last_date(log_file->prefix, log_file->ext);

    if (log_file->open_date == 0) {
        log_file->open_date = time(NULL);
    }

    log_file->max_size = 0;
    log_file->rotate_date = 0;

    log_file->_internal = log_file_open_new(log_file, log_file->open_date);
    log_check_max_files(log_file);

    if (!log_file->_internal) {
        e_trace(1, "Could not open first log file");
        log_file_delete(&log_file);
        return NULL;
    }

    return log_file;
}

void log_file_close(log_file_t **log_file)
{
    log_file_delete(log_file);
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
    file->rotate_date =
        file->open_date + file->rotate_delay;
}

static void log_file_rotate(log_file_t *file, time_t now)
{
    p_fclose(&file->_internal);
    file->_internal = log_file_open_new(file, now);
    file->open_date = now;
    log_check_max_files(file);
}

static int log_check_rotate(log_file_t *log_file)
{
    if (log_file->max_size > 0) {
        struct stat stats;

        fstat(fileno(log_file->_internal), &stats);

        if (stats.st_size >= log_file->max_size) {
            log_file_rotate(log_file, time(NULL));
            if (!log_file->_internal) {
                e_trace(1, "Could not rotate");
                return 1;
            }
        }
    }
    if (log_file->rotate_delay > 0) {
        time_t now = time(NULL);

        if (log_file->rotate_date <= now) {
            log_file_rotate(log_file, now);

            if (!log_file->_internal) {
                e_trace(1, "Could not rotate");
                return 1;
            }
            log_file->rotate_date =
                log_file->open_date + log_file->rotate_delay;
        }
    }

    return 0;
}

size_t log_fwrite(const void *buf, size_t size, size_t nmemb,
                  log_file_t *log_file)
{
    assert (log_file);

    if (log_check_rotate(log_file)) {
        return 0;
    }

    return fwrite(buf, size, nmemb, log_file->_internal);
}

int log_fprintf(log_file_t *log_file, const char *format, ...)
{
    int res;
    va_list args;

    assert (log_file);

    if (log_check_rotate(log_file)) {
        return 0;
    }

    va_start(args, format);
    res = vfprintf(log_file->_internal, format, args);
    va_end(args);

    return res;
}

int log_file_flush(log_file_t *log_file)
{
    if (log_file->_internal) {
        return fflush(log_file->_internal);
    }
    return 0;
}
#endif
