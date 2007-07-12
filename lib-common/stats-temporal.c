/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <sys/time.h>
#include <linux/limits.h>
#include <time.h>
#include <sys/mman.h>
#include <glob.h>

#include <lib-common/blob.h>
#include <lib-common/mmappedfile.h>
#include <lib-common/array.h>
#include <lib-common/timeval.h>

#include "stats-temporal.h"

/**************************
 *
 *  Stats nb msgs per sec
 *
 */

typedef struct stats_file32 {
    int start_time;
    int end_time;
    int nb_allocated;
    int nb_stats;
    uint32_t stats[];
} stats_file32;

typedef struct stats_file64 {
    int start_time;
    int end_time;
    int nb_allocated;
    int nb_stats;
    uint64_t stats[];
} stats_file64;

typedef struct mmfile_stats32 MMFILE_ALIAS(stats_file32) mmfile_stats32;
MMFILE_FUNCTIONS(mmfile_stats32, mmfile_stats32);
typedef struct mmfile_stats64 MMFILE_ALIAS(stats_file64) mmfile_stats64;
MMFILE_FUNCTIONS(mmfile_stats64, mmfile_stats64);

struct stats_temporal_t {
    char path[PATH_MAX];
    mmfile_stats32 *per_sec;
    mmfile_stats64 *per_hour;
    bool do_sec;
    bool do_hour;
    int nb_stats;
};

#define PER_SEC_NB_SECONDS   (3600 * 8)
#define PER_HOUR_NB_HOURS    (24 * 7)
#define PER_HOUR_NB_SECONDS  (3600 * PER_HOUR_NB_HOURS)

/* Compute the aligned boundaries in local time for a range of hours
 * around a given UTC date.  Optionally return the tm struct for the
 * start date.
 */
static void compute_hour_range(time_t date, int hours,
                               struct tm *tp,
                               time_t *start, time_t *end)
{
    struct tm tt;

    /* This code works only for slots of at least 2 hours, to produce
     * non empty ranges for dates close to the time changes
     */
    assert (hours > 1);
    localtime_r(&date, &tt);
    tt.tm_hour -= tt.tm_hour % hours;
    tt.tm_min = tt.tm_sec = 0;
    *start = mktime(&tt);
    if (tp) {
        /* return tm struct for start time */
        *tp = tt;
    }
    tt.tm_hour += hours;
    *end = mktime(&tt);
}

/* Compute the aligned boundaries in local time for a range of hours
 * around a given UTC date.  Optionally return the tm struct for the
 * start date.
 */
static void compute_day_range(time_t date, int days,
                              struct tm *tp,
                              time_t *start, time_t *end)
{
    struct tm tt;

    if (days <= 1) {
        compute_hour_range(date, 24, tp, start, end);
        return;
    }

#if 0
    /* FIXME: This code is buggy : it computes day of week in GMT time,
     * instead of local time. */
    /* Start on mondays 0:00:00 (1/1/1970 was a Thursday) */
    time_t dow = ((date / 86400) + 3) % days;
    time_t date0 = date - dow * 86400;
    localtime_r(&date0, &tt);
#else
    assert (days == 7);
    localtime_r(&date, &tt);
    tt.tm_mday -= (tt.tm_wday + 6) % 7;
    e_trace(2, "found mday: %d", tt.tm_mday);
#endif
    tt.tm_hour = tt.tm_min = tt.tm_sec = 0;
    *start = mktime(&tt);
    if (tp) {
        /* return tm struct for start time */
        *tp = tt;
    }
    tt.tm_mday += days;
    *end = mktime(&tt);
}

static mmfile_stats32 *per_sec_file_initialize(const char *prefix,
                                               time_t date, int nb_stats,
                                               bool autocreate)
{
    char buf[PATH_MAX];
    mmfile_stats32 *m;
    struct tm t;
    time_t start_time, end_time;

    compute_hour_range(date, PER_SEC_NB_SECONDS / 3600,
                       &t, &start_time, &end_time);

    snprintf(buf, sizeof(buf), "%s_sec_%04d%02d%02d_%02d%02d%02d.bin",
             prefix, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);

    m = mmfile_stats32_open(buf, O_RDWR);
    if (m && (m->area->start_time > m->area->end_time ||
              m->area->nb_stats != nb_stats)) {
        /* Inconsistent header, probably an older version */
        mmfile_stats32_close(&m);
        unlink(buf);
    }
    if (!m && autocreate) {
        int to_allocate = end_time - start_time;

        e_trace(2, "Could not mmfile_open %s: %m, try to create...", buf);
        m = mmfile_stats32_creat(buf, sizeof(stats_file32) +
                                 to_allocate * nb_stats * sizeof(uint32_t));
        if (!m) {
            /* FIXME: This message may be repeated many times if stat file
             * cannot be created.
             */
            e_error("Could not mmfile_creat %s: %m.", buf);
            return NULL;
        }

        m->area->start_time = start_time;
        m->area->end_time = end_time;
        m->area->nb_allocated = to_allocate;
        m->area->nb_stats = nb_stats;

        e_trace(1, "Mapped file created (file = %s) (allocated = %d)",
                buf, to_allocate);
    }

    return m;
}

static mmfile_stats64 *per_hour_file_initialize(const char *prefix,
                                                time_t date, int nb_stats,
                                                bool autocreate)
{
    char buf[PATH_MAX];
    mmfile_stats64 *m;
    struct tm t;
    time_t start_time, end_time;

    /* compute boundaries for the target day */
    compute_day_range(date, PER_HOUR_NB_HOURS / 24,
                      &t, &start_time, &end_time);

    snprintf(buf, sizeof(buf), "%s_hour_%04d%02d%02d_%02d%02d%02d.bin",
             prefix, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);

    e_trace(2, "for start = %d : opening %s...", (int)date, buf);
    m = mmfile_stats64_open(buf, O_RDWR);
    if (m && (m->area->start_time > m->area->end_time ||
              m->area->nb_stats != nb_stats)) {
        /* Inconsistent header, probably an older version */
        mmfile_stats64_close(&m);
        unlink(buf);
    }
    if (!m && autocreate) {
        int to_allocate = (end_time - start_time) / 3600;

        e_trace(2, "Could not mmfile_open (%m), try to create...");
        m = mmfile_stats64_creat(buf, sizeof(stats_file64) +
                                 to_allocate * nb_stats * sizeof(uint64_t));
        if (!m) {
            /* FIXME: This message may be repeated many times if stat file
             * cannot be created.
             */
            e_error("Could not mmfile_creat (%m) !!...");
            return NULL;
        }

        m->area->start_time = start_time;
        m->area->end_time = end_time;
        m->area->nb_allocated = to_allocate;
        m->area->nb_stats = nb_stats;

        e_trace(1, "Mapped file created (file = %s) (allocated = %d)",
                buf, to_allocate);
    }

    return m;
}

#if 0
static int find_last_file(const char *path, bool sec)
{
    char glob_path[PATH_MAX];
    glob_t files;
    struct tm t;
    int res = -1;

    p_clear(&files, 1);
    p_clear(&t, 1);
    t.tm_isdst = -1;
    if (sec) {
        snprintf(glob_path, sizeof(glob_path), "%s_sec_*.bin", path);
    } else {
        snprintf(glob_path, sizeof(glob_path), "%s_hour_*.bin", path);
    }

    if (glob(glob_path, 0, NULL, &files) || files.gl_pathc == 0) {
        goto exit;
    }

    if (sec) {
        if (sscanf(files.gl_pathv[files.gl_pathc - 1] + strlen(path),
                   "_sec_%04d%02d%02d_%02d%02d%02d.bin",
                   &t.tm_year, &t.tm_mon, &t.tm_mday,
                   &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
        {
            goto exit;
        }
    } else {
        if (sscanf(files.gl_pathv[files.gl_pathc - 1] + strlen(path),
                   "_hour_%04d%02d%02d_%02d%02d%02d.bin",
                   &t.tm_year, &t.tm_mon, &t.tm_mday,
                   &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
        {
            goto exit;
        }
    }

    t.tm_year -= 1900;
    t.tm_mon  -= 1;

    res = mktime(&t);

  exit:
    globfree(&files);
    return res;
}
#endif

stats_temporal_t *stats_temporal_new(const char *path, int nb_stats,
                                     int flags)
{
    stats_temporal_t* stats;
    
    /* No longer need to open existing stats file, it will be open upon
     * first actual use
     */
    stats = p_new(stats_temporal_t, 1);
    pstrcpy(stats->path, sizeof(stats->path), path);
    stats->nb_stats = nb_stats;
    stats->do_sec  = !!(flags & STATS_TEMPORAL_SECONDS);
    stats->do_hour = !!(flags & STATS_TEMPORAL_HOURS);

    return stats;
}

static inline void stats_temporal_wipe(stats_temporal_t *stats)
{
    mmfile_stats32_close(&stats->per_sec);
    mmfile_stats64_close(&stats->per_hour);
}
DO_DELETE(stats_temporal_t, stats_temporal);

bool stats_temporal_shrink(stats_temporal_t *stats, int date)
{
    int offset;

    if (stats->do_sec && stats->per_sec) {
        /* Keep the last sec stat */
        offset = date - stats->per_sec->area->start_time + 1;

        if (0 <= offset && offset < stats->per_sec->area->nb_allocated) {
            if (mmfile_stats32_truncate(stats->per_sec,
                                        sizeof(stats_file32) +
                                        offset * stats->nb_stats *
                                        sizeof(uint32_t)))
            {
                return false;
            }
            stats->per_sec->area->nb_allocated = offset;
        }
    }
    if (stats->do_hour && stats->per_hour) {
        /* Keep last hour in range */
        offset = (date - stats->per_hour->area->start_time) / 3600 + 1;

        if (0 <= offset && offset < stats->per_sec->area->nb_allocated) {
            if (mmfile_stats64_truncate(stats->per_hour,
                                        sizeof(stats_file64) +
                                        offset * stats->nb_stats *
                                        sizeof(uint64_t)))
            {
                return false;
            }
            stats->per_hour->area->nb_allocated = offset;
        }
    }

    return true;
}

static int stats_temporal_log_one_sec(stats_temporal_t *stats,
                                      int date, int index, int incr)
{
    int offset, range;

    if (stats->per_sec) {
        /* Check if date is within range of current stat file */
        if (date < stats->per_sec->area->start_time
        ||  date >= stats->per_sec->area->end_time) {
            mmfile_stats32_close(&stats->per_sec);
        }
    }

    if (!stats->per_sec) {
        stats->per_sec = per_sec_file_initialize(stats->path, date,
                                                 stats->nb_stats, true);
        if (!stats->per_sec) {
            return -1;
        }
    }

    offset = date - stats->per_sec->area->start_time;
    range = stats->per_sec->area->end_time -
            stats->per_sec->area->start_time;

    if (offset < 0 || offset >= range) {
        /* Inconsistent stats file */
        return -1;
    }

    if (offset >= stats->per_sec->area->nb_allocated) {
        /* Previously truncated file */
        if (mmfile_stats32_truncate(stats->per_sec,
                                    sizeof(stats_file32) +
                                    range * stats->nb_stats *
                                    sizeof(uint32_t)))
        {
            /* Failed to truncate to new size */
            return -1;
        }
        stats->per_sec->area->nb_allocated = range;
    }
    stats->per_sec->area->stats[offset * stats->per_sec->area->nb_stats +
                                index] += incr;

    return 0;
}

static int stats_temporal_log_one_hour(stats_temporal_t *stats,
                                       int date, int index, int incr)
{
    int offset, range;

    if (stats->per_hour) {
        /* Check if date is within range of current stat file */
        if (date < stats->per_hour->area->start_time
        ||  date >= stats->per_hour->area->end_time) {
            mmfile_stats64_close(&stats->per_hour);
        }
    }

    if (!stats->per_hour) {
        stats->per_hour = per_hour_file_initialize(stats->path, date,
                                                   stats->nb_stats, true);
        if (!stats->per_hour) {
            return -1;
        }
    }

    offset = (date - stats->per_hour->area->start_time) / 3600;
    range = (stats->per_hour->area->end_time -
             stats->per_hour->area->start_time) / 3600;

    if (offset < 0 || offset >= range) {
        /* Inconsistent stats file */
        return -1;
    }

    if (offset >= stats->per_hour->area->nb_allocated) {
        /* Previously truncated file */
        if (mmfile_stats64_truncate(stats->per_hour,
                                    sizeof(stats_file64) +
                                    range * stats->nb_stats *
                                    sizeof(uint64_t)))
        {
            /* Failed to truncate to new size */
            return -1;
        }
        stats->per_hour->area->nb_allocated = range;
    }
    stats->per_hour->area->stats[offset * stats->per_hour->area->nb_stats +
                                 index] += incr;

    return 0;
}

int stats_temporal_log_one(stats_temporal_t *stats, time_t date,
                           int index, int incr)
{
    int status = 0;

    if (stats->do_sec) {
        if (stats_temporal_log_one_sec(stats, date, index, incr))
            status = -1;
    }
    if (stats->do_hour) {
        if (stats_temporal_log_one_hour(stats, date, index, incr))
            status = -1;
    }
    return status;
}

/***********************************
 *
 *  Stats querying
 *
 */
#define OUTPUT_SEC_MAX_NB   3600
#define OUTPUT_HOUR_MAX_NB  (24 * 7)

static void stats_query_read_sec(stats_temporal_t *stats, int start_time,
                                 blob_t *blob, int **outp,
                                 int start, int end, bfield_t *mask)
{
    int i, j, offset, stop;
    int nb_stats = stats->nb_stats;
    mmfile_stats32 *msec;
    int val;
    int *out = *outp;

    if (stats->per_sec && start_time == stats->per_sec->area->start_time) {
        /* We can use the currently opened stats file */
        msec = stats->per_sec;
    } else {
        msec = per_sec_file_initialize(stats->path, start, nb_stats, false);
    }
    if (msec) {
        start_time = msec->area->start_time;
        stop = MIN(end, start_time + msec->area->nb_allocated);
        for (i = start; i < stop; i++) {
            if (blob) {
                blob_append_fmt(blob, "%d", i);
            } else {
                *out++ = i;
            }
            offset = (i - start_time) * nb_stats;
            for (j = 0; j < nb_stats; j++) {
                if (!mask || bfield_isset(mask, j)) {
                    val = msec->area->stats[offset + j];
                    if (blob) {
                        blob_append_fmt(blob, " %d", val);
                    } else {
                        *out++ = val;
                    }
                }
            }
            if (blob) {
                blob_append_byte(blob, '\n');
            }
        }
        if (msec != stats->per_sec) {
            mmfile_stats32_close(&msec);
        }
        start = stop;
    }
    if (start < end) {
        if (blob) {
            for (i = start; i < end; i++) {
                blob_append_fmt(blob, "%d", i);
                for (j = 0; j < nb_stats; j++) {
                    if (!mask || bfield_isset(mask, j)) {
                        blob_append_cstr(blob, " 0");
                    }
                }
                blob_append_byte(blob, '\n');
            }
        } else {
            for (i = start; i < end; i++) {
                *out++ = i;
                for (j = 0; j < nb_stats; j++) {
                    if (!mask || bfield_isset(mask, j)) {
                        *out++ = 0;
                    }
                }
            }
        }
    }
    *outp = out;
}

static void stats_query_read_hour(stats_temporal_t *stats, int start_time,
                                  blob_t *blob, int64_t **outp,
                                  int start, int end, bfield_t *mask)
{
    int i, j, offset, stop;
    int nb_stats = stats->nb_stats;
    mmfile_stats64 *mhour;
    int64_t val;
    int64_t *out = *outp;

    if (stats->per_hour && start_time == stats->per_hour->area->start_time) {
        /* We can use the currently opened stats file */
        mhour = stats->per_hour;
    } else {
        mhour = per_hour_file_initialize(stats->path, start, nb_stats, false);
    }
    if (mhour) {
        start_time = mhour->area->start_time;
        stop = MIN(end, start_time + mhour->area->nb_allocated * 3600);
        e_trace(2, "Reading file %d between %d and %d", start_time, start, end);
        for (i = start; i < stop; i += 3600) {
            if (blob) {
                blob_append_fmt(blob, "%d", i);
            } else {
                *out++ = i;
            }
            offset = ((i - start_time) / 3600) * nb_stats;
            for (j = 0; j < nb_stats; j++) {
                if (!mask || bfield_isset(mask, j)) {
                    val = mhour->area->stats[offset + j];
                    if (blob) {
                        blob_append_fmt(blob, " %lld", (long long)val);
                    } else {
                        *out++ = val;
                    }
                }
            }
            if (blob) {
                blob_append_byte(blob, '\n');
            }
        }
        if (mhour != stats->per_hour) {
            mmfile_stats64_close(&mhour);
        }
        start = stop;
    }
    if (start < end) {
        e_trace(2, "Appending dummy data between %d and %d", start, end);
        if (blob) {
            for (i = start; i < end; i += 3600) {
                blob_append_fmt(blob, "%d", i);
                for (j = 0; j < nb_stats; j++) {
                    if (!mask || bfield_isset(mask, j)) {
                        blob_append_cstr(blob, " 0");
                    }
                }
                blob_append_byte(blob, '\n');
            }
        } else {
            for (i = start; i < end; i += 3600) {
                *out++ = i;
                for (j = 0; j < nb_stats; j++) {
                    if (!mask || bfield_isset(mask, j)) {
                        *out++ = 0;
                    }
                }
            }
        }
    }
    *outp = out;
}

int stats_temporal_query_sec(stats_temporal_t *stats, blob_t *blob,
                             int *outp, int start, int nb_values,
                             bfield_t *mask)
{
    int end;
    time_t start_time, end_time;

    if (!stats->do_sec) {
        if (blob) {
            blob_append_cstr(blob,
                             "stats per second deactivated for these statistics");
        }
        return -1;
    }

    if (nb_values > OUTPUT_SEC_MAX_NB) {
        e_trace(2, "Truncated nb_values from %d to %d",
                nb_values, OUTPUT_SEC_MAX_NB);
        nb_values = OUTPUT_SEC_MAX_NB;
    }

    end = start + nb_values;
    e_trace(2, "Getting stats between %d and %d", start, end);

    while (start < end) {
        /* compute boundaries for the sec stats file at start time */
        compute_hour_range(start, PER_SEC_NB_SECONDS / 3600,
                           NULL, &start_time, &end_time);

        if (start < start_time || start >= end_time) {
            /* Safety check: avoid infinite loop if compute_hour_range
             * fails
             */
            return -1;
        }
        stats_query_read_sec(stats, start_time, blob, &outp,
                             start, MIN(end, end_time), mask);

        start = end_time;
    }

    return 0;
}

int stats_temporal_query_hour(stats_temporal_t *stats, blob_t *blob,
                              int64_t *outp, int start, int nb_values,
                              bfield_t *mask)
{
    int end;
    time_t start_time, end_time;

    if (!stats->do_hour) {
        if (blob) {
            blob_append_cstr(blob,
                             "stats per hour deactivated for these statistics");
        }
        return -1;
    }

    if (nb_values > OUTPUT_HOUR_MAX_NB) {
        e_trace(2, "Truncated nb_values from %d to %d",
                nb_values, OUTPUT_HOUR_MAX_NB);
        nb_values = OUTPUT_HOUR_MAX_NB;
    }

    /* Round date to the current hour */
    start -= start % 3600;
    end = start + nb_values * 3600;
    e_trace(2, "Getting stats between %d and %d (nb_values = %d)",
            start, end, nb_values);

    while (start < end) {
        /* compute boundaries for the hour stats file at start time */
        compute_day_range(start, PER_HOUR_NB_HOURS / 24,
                          NULL, &start_time, &end_time);

        if (start < start_time || start >= end_time) {
            /* Safety check: avoid infinite loop if compute_hour_range
             * fails
             */
            return -1;
        }
        stats_query_read_hour(stats, start_time, blob, &outp,
                              start, MIN(end, end_time), mask);

        start = end_time;
    }

    return 0;
}
