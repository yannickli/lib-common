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
#include <unistd.h>
#include <glob.h>

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

static int build_real_path(char *buf, int size,
                           const char *prefix, time_t date)
{
    struct tm *cur_date;

    cur_date = localtime(&date);
    return snprintf(buf, size, "%s_%04d%02d%02d_%02d%02d%02d.log",
                    prefix,
                    cur_date->tm_year + 1900,
                    cur_date->tm_mon + 1,
                    cur_date->tm_mday,
                    cur_date->tm_hour,
                    cur_date->tm_min,
                    cur_date->tm_sec);
}


static FILE *log_file_open_new(const char *prefix, time_t date)
{
    char real_path[PATH_MAX];
    char sym_path[PATH_MAX];
    int len;
    FILE *res;

    len = build_real_path(real_path, sizeof(real_path), prefix, date);

    if (len >= ssizeof(real_path)) {
        e_trace(1, "Path too long");
        return NULL;
    }

    res = fopen(real_path, "a");
    if (!res) {
        e_trace(1, "Could not open log file: %s (%m)", real_path);
    }

    /* Add a symlink */
    len = snprintf(sym_path, sizeof(sym_path), "%s_last.log", prefix);
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

static int log_last_date(const char *prefix)
{
    char path[PATH_MAX];
    const char *p;
    char sympath[PATH_MAX];
    ssize_t len;
    struct tm cur_date;

    /* Try to find the last log file */
    len = snprintf(path, sizeof(path), "%s_last.log", prefix);
    if (len >= ssizeof(path)) {
        e_trace(1, "Last log file path too long");
        return 0;
    }

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
    len = sscanf(p, "%4d%2d%2d_%2d%2d%2d.log",
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

#define LOG_SUFFIX_LEN  strlen("YYYYMMDD_HHMMSS.log")

static void log_check_max_files(log_file_t *log_file)
{
    glob_t globbuf;

    if (log_file->max_files > 0) {
        char buf[PATH_MAX];
        int len_file, len_prefix, dl;
        int nb_file = 0;

        len_prefix = strlen(log_file->prefix) + 1;

        snprintf(buf, sizeof(buf), "%s_*",log_file->prefix);
        glob(buf, 0, NULL, &globbuf);

        for (int i = 0; i < (int)globbuf.gl_pathc; i++) {
            len_file = strlen(globbuf.gl_pathv[i]);

            if (len_file - len_prefix == LOG_SUFFIX_LEN) {
                nb_file++;
            }
        }
        dl = nb_file - log_file->max_files;
        if (dl < 0) {
            goto end;
        }
        for (int i = 0; dl && i < (int)globbuf.gl_pathc; i++) {
            len_file = strlen(globbuf.gl_pathv[i]);

            if (len_file - len_prefix == LOG_SUFFIX_LEN) {
                unlink(globbuf.gl_pathv[i]);
                dl--;
            }
        }
end:
        globfree(&globbuf);
    }
}

log_file_t *log_file_open(const char *prefix)
{
    log_file_t *log_file;
    ssize_t len;

    log_file = log_file_new();

    len = pstrcpy(log_file->prefix, sizeof(log_file->prefix), prefix);
    if (len >= ssizeof(log_file->prefix)) {
        e_trace(1, "Path format too long");
        log_file_delete(&log_file);
        return NULL;
    }

    log_file->open_date = log_last_date(prefix);

    if (log_file->open_date == 0) {
        log_file->open_date = time(NULL);
    }

    log_file->max_size = 0;
    log_file->rotate_date = 0;

    log_file->_internal = log_file_open_new(log_file->prefix,
                                            log_file->open_date);
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

    file->_internal = log_file_open_new(file->prefix, now);
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
