/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <sys/stat.h>

#include <lib-common/mem.h>
#include <lib-common/blob.h>
#include <lib-common/macros.h>
#include <lib-common/string_is.h>

#include "log_file.h"

GENERIC_INIT(log_file_t, log_file);
static void log_file_wipe(log_file_t *log_file)
{
    if (log_file->_internal) {
        fclose(log_file->_internal);
    }
    p_clear(log_file, 1);
}
GENERIC_NEW(log_file_t, log_file);
GENERIC_DELETE(log_file_t, log_file);

/* log file names should depend on rotation scheme: slower rotation
 * scheme should shorten log filename so reopening it yields the same
 * file
 */

static inline int build_real_path(char *buf, int size,
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


static inline FILE *log_file_open_new(const char *prefix, time_t date)
{
    char real_path[PATH_MAX];
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

    return res;
}

log_file_t *log_file_open(const char *prefix)
{
    log_file_t* log_file;
    ssize_t len;

    log_file = log_file_new();

    len = pstrcpy(log_file->prefix, sizeof(log_file->prefix), prefix);
    if (len >= ssizeof(log_file->prefix)) {
        e_trace(1, "Path format too long");
        log_file_delete(&log_file);
        return NULL;
    }

    log_file->open_date = time(NULL);
    log_file->max_size = 0;
    log_file->rotate_date = 0;

    log_file->_internal = log_file_open_new(log_file->prefix,
                                            log_file->open_date);

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

void log_file_set_rotate_delay(log_file_t *file, time_t delay)
{
    file->rotate_delay = delay;
    file->rotate_date =
        file->open_date + file->rotate_delay;
}

static inline void log_file_rotate(log_file_t *file, time_t now)
{
    fclose(file->_internal);

    file->_internal = log_file_open_new(file->prefix, now);
    file->open_date = now;
}


static inline int log_check_rotate(log_file_t *log_file)
{
    bool rotated = false;
    if (log_file->max_size > 0) {
        struct stat stats;

        fstat(fileno(log_file->_internal), &stats);

        if (stats.st_size >= log_file->max_size) {
            log_file_rotate(log_file, time(NULL));
            if (!log_file->_internal) {
                e_debug(1, "Could not rotate");
                return 1;
            }
            rotated = true;
        }
    }
    if (log_file->rotate_delay > 0) {
        time_t now = time(NULL);

        if (log_file->rotate_date <= now) {
            log_file_rotate(log_file, now);

            if (!log_file->_internal) {
                e_debug(1, "Could not rotate");
                return 1;
            }
            log_file->rotate_date =
                log_file->open_date + log_file->rotate_delay;

            rotated = true;
        }
    }

    return 0;
}

size_t log_fwrite(const void *buf, size_t size, size_t nmemb,
                  log_file_t *log_file)
{
    if (log_check_rotate(log_file)) {
        return 0;
    }

    return fwrite(buf, size, nmemb, log_file->_internal);
}

int log_fprintf(log_file_t *log_file, const char *format, ...)
{
    int res;
    va_list args;
    
    if (log_check_rotate(log_file)) {
        return 0;
    }

    va_start(args, format);
    res = vfprintf(log_file->_internal, format, args);
    va_end(args);

    return res;
}

int log_flush(log_file_t *log_file)
{
    if (log_file->_internal) {
        return fflush(log_file->_internal);
    }
    return 0;
}
