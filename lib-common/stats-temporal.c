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

#include <linux/limits.h>
#include <sys/mman.h>
#include <glob.h>
#include "all.h"

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

typedef struct stats_stage {
    int32_t scale;
    int32_t count;
    int32_t current;
    int32_t pos;
    int32_t incr;
    int32_t offset;
    int32_t pad1;
    int32_t pad2;
} stats_stage;

typedef struct stats_file_auto {
#define STATS_AUTO_MAGIC 0x51A10000
    int32_t magic;
    int32_t nb_stages;
    int32_t nb_allocated;
    int32_t nb_stats;
    int32_t pad[4];
    stats_stage stage[STATS_STAGE_MAX];
} stats_file_auto;

typedef struct mmfile_stats32 MMFILE_ALIAS(stats_file32) mmfile_stats32;
MMFILE_FUNCTIONS(mmfile_stats32, mmfile_stats32);
typedef struct mmfile_stats64 MMFILE_ALIAS(stats_file64) mmfile_stats64;
MMFILE_FUNCTIONS(mmfile_stats64, mmfile_stats64);
typedef struct mmfile_stats_auto MMFILE_ALIAS(stats_file_auto) mmfile_stats_auto;
MMFILE_FUNCTIONS(mmfile_stats_auto, mmfile_stats_auto);

struct stats_temporal_t {
    char path[PATH_MAX];
    mmfile_stats32 *per_sec;
    mmfile_stats64 *per_hour;
    mmfile_stats_auto *file;
    bool do_sec;
    bool do_hour;
    int nb_stats;
    int nb_stages;
    int desc[STATS_STAGE_MAX * 2];
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
    if (start)
        *start = mktime(&tt);
    if (tp) {
        /* return tm struct for start time */
        *tp = tt;
    }
    if (end) {
        tt.tm_hour += hours;
        *end = mktime(&tt);
    }
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
    if (start)
        *start = mktime(&tt);
    if (tp) {
        /* return tm struct for start time */
        *tp = tt;
    }
    if (end) {
        tt.tm_mday += days;
        *end = mktime(&tt);
    }
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

    m = mmfile_stats32_open(buf, O_RDWR, 0, 0);
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
    compute_day_range(date, 7, &t, &start_time, &end_time);

    snprintf(buf, sizeof(buf), "%s_hour_%04d%02d%02d_%02d%02d%02d.bin",
             prefix, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);

    e_trace(2, "for start = %d : opening %s...", (int)date, buf);
    m = mmfile_stats64_open(buf, O_RDWR, 0, 0);
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

static mmfile_stats_auto *auto_file_initialize(stats_temporal_t *stats,
                                               time_t date, bool autocreate)
{
    char buf[PATH_MAX];
    mmfile_stats_auto *m;
    stats_stage *st;

    snprintf(buf, sizeof(buf), "%s_auto.bin", stats->path);

    e_trace(2, "opening %s...", buf);
    m = mmfile_stats_auto_open(buf, O_RDWR, 0, 0);
    if (m && (m->area->magic != STATS_AUTO_MAGIC ||
              m->area->nb_stats != stats->nb_stats ||
              m->area->nb_stages != stats->nb_stages)) {
        /* Inconsistent header, probably an older version */
        mmfile_stats_auto_close(&m);
        unlink(buf);
    }
    if (!m && autocreate) {
        int to_allocate, i, offset;

        to_allocate = sizeof(stats_file_auto);
        to_allocate = (to_allocate + 255) & ~255;
        to_allocate += stats->nb_stats * stats->desc[1] * sizeof(uint32_t);
        to_allocate = (to_allocate + 255) & ~255;

        for (i = 1; i < stats->nb_stages; i++) {
            to_allocate += stats->nb_stats * stats->desc[2 * i + 1] *
                           sizeof(uint64_t);
            to_allocate = (to_allocate + 255) & ~255;
        }

        e_trace(2, "Could not mmfile_open (%m), try to create...");
        m = mmfile_stats_auto_creat(buf, to_allocate);
        if (!m) {
            /* FIXME: This message may be repeated many times if stat file
             * cannot be created.
             */
            e_error("Could not mmfile_creat (%m) !!...");
            return NULL;
        }

        m->area->magic = STATS_AUTO_MAGIC;
        m->area->nb_stages = stats->nb_stages;
        m->area->nb_allocated = to_allocate;
        m->area->nb_stats = stats->nb_stats;

        offset = sizeof(stats_file_auto);
        offset = (offset + 255) & ~255;

        st = m->area->stage;
        st->scale = 1;
        st->count = stats->desc[1];
        st->current = date;
        st->pos = 0;
        st->incr = stats->nb_stats * sizeof(uint32_t);
        st->offset = offset;

        offset += st->count * st->incr;
        offset = (offset + 255) & ~255;

        for (i = 1, st++; i < stats->nb_stages; i++, st++) {
            st->scale = stats->desc[2 * i];
            st->count = stats->desc[2 * i + 1];
            st->current = date / st->scale;
            st->pos = 0;
            st->incr = stats->nb_stats * sizeof(uint64_t);
            st->offset = offset;
            offset += st->count * st->incr;
            offset = (offset + 255) & ~255;
        }

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
                                     int flags, const int *desc)
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
    stats->nb_stages = flags >> 2;
    if (stats->nb_stages > 0 && stats->nb_stages <= STATS_STAGE_MAX) {
        if (desc) {
            memcpy(stats->desc, desc, stats->nb_stages * 2 * sizeof(*desc));
        } else {
            /* Use default descriptors:
             * 1 hour at second, 1 day at minute, 1 week at hour precision
             */
            static int default_desc[] = { 1, 3600, 60, 24 * 60, 3600, 7 * 24 };
            memcpy(stats->desc, default_desc, sizeof(default_desc));
            stats->nb_stages = 3;
        }
    }
    return stats;
}

static inline void stats_temporal_wipe(stats_temporal_t *stats)
{
    mmfile_stats32_close(&stats->per_sec);
    mmfile_stats64_close(&stats->per_hour);
    mmfile_stats_auto_close(&stats->file);
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

/* This has to be a macro, as real_value can be uint32_t* or uint64_t* */
#define STAT_UPD_VALUE(type, real_value, real_value_bis, value)        \
    switch (type) {                                                    \
      case STATS_UPD_INCR:                                             \
        *(real_value) += value;                                        \
        break;                                                         \
                                                                       \
      case STATS_UPD_MIN:                                              \
        if (*(real_value) == 0) {                                      \
            /* FIXME: We don't known if a value was previously         \
             * stored here, so 0 is considered as NULL */              \
            *(real_value) = value;                                     \
        } else {                                                       \
            *(real_value) = MIN(*(real_value), (unsigned int)(value)); \
        }                                                              \
        break;                                                         \
                                                                       \
      case STATS_UPD_MAX:                                              \
        *(real_value) = MAX(*(real_value), (unsigned int)(value));     \
        break;                                                         \
                                                                       \
      case STATS_UPD_MEAN:                                             \
        *(real_value) = (*(real_value) * *(real_value_bis) + (value))  \
                    / (*(real_value_bis) + 1);                         \
        *(real_value_bis) += 1;                                        \
        break;                                                         \
    }


static int stats_temporal_upd_sec(stats_temporal_t *stats, int date,
                                  stats_upd_type type,
                                  int index, int index_bis,
                                  int value)
{
    int offset, range;
    uint32_t *real_value;
    uint32_t *real_value_bis;

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
    real_value     = &stats->per_sec->area->stats[offset *
        stats->per_sec->area->nb_stats + index];
    real_value_bis = &stats->per_sec->area->stats[offset *
        stats->per_sec->area->nb_stats + index_bis];

    STAT_UPD_VALUE(type, real_value, real_value_bis, value);

    return 0;
}

static int stats_temporal_upd_hour(stats_temporal_t *stats, int date,
                                   stats_upd_type type,
                                   int index, int index_bis,
                                   int value)
{
    int offset, range;
    uint64_t *real_value;
    uint64_t *real_value_bis;

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

    real_value     = &stats->per_hour->area->stats[offset *
        stats->per_hour->area->nb_stats + index];
    real_value_bis = &stats->per_hour->area->stats[offset *
        stats->per_hour->area->nb_stats + index_bis];

    STAT_UPD_VALUE(type, real_value, real_value_bis, value);

    return 0;
}

static int stats_temporal_upd_auto(stats_temporal_t *stats, int date,
                                   stats_upd_type type,
                                   int index, int index_bis,
                                   int value)
{
    stats_stage *st;
    uint32_t *vp32;
    uint64_t *vp64;
    int i, date1;

    if (!stats->file) {
        stats->file = auto_file_initialize(stats, date, true);
        if (!stats->file) {
            return -1;
        }
    }

    st = stats->file->area->stage;
    if (date > st->current) {
        if (date >= st->current + st->count) {
            /* drop all stats for this stage */
            st->current = date;
            st->pos = 0;
            vp32 = (uint32_t *)((byte *)stats->file->area + st->offset);
            memset(vp32, 0, st->count * st->incr);
        } else {
            while (st->current < date) {
                st->current++;
                if (++st->pos == st->count) {
                    st->pos = 0;
                }
                vp32 = (uint32_t *)((byte *)stats->file->area + st->offset +
                                    st->pos * st->incr);
                memset(vp32, 0, st->incr);
            }
        }
        for (i = 1, st++; i < stats->nb_stages; i++, st++) {
            date1 = date / st->scale;
            if (date1 == st->current)
                break;
            if (date1 >= st->current + st->count) {
                /* drop all stats for this stage */
                st->current = date1;
                st->pos = 0;
                vp64 = (uint64_t *)((byte *)stats->file->area + st->offset);
                memset(vp64, 0, st->count * st->incr);
            } else {
                while (st->current < date1) {
                    st->current++;
                    if (++st->pos == st->count) {
                        st->pos = 0;
                    }
                    vp64 = (uint64_t *)((byte *)stats->file->area + st->offset +
                                        st->pos * st->incr);
                    memset(vp64, 0, st->incr);
                }
            }
        }
        st = stats->file->area->stage;
    }
    if (date == st->current) {
        /* simple case, current slot for all stages */
        vp32 = (uint32_t *)((byte *)stats->file->area + st->offset +
                            st->pos * st->incr);
        STAT_UPD_VALUE(type, vp32 + index, vp32 + index_bis, value);
        for (i = 1, st++; i < stats->nb_stages; i++, st++) {
            vp64 = (uint64_t *)((byte *)stats->file->area + st->offset +
                                st->pos * st->incr);
            STAT_UPD_VALUE(type, vp64 + index, vp64 + index_bis, value);
        }
    } else {
        if (date > st->current - st->count) {
            vp32 = (uint32_t *)((byte *)stats->file->area + st->offset +
                                (st->pos + st->count + date - st->current) % st->count *
                                st->incr);
            STAT_UPD_VALUE(type, vp32 + index, vp32 + index_bis, value);
        }
        for (i = 1, st++; i < stats->nb_stages; i++, st++) {
            date1 = date / st->scale;
            if (date1 > st->current - st->count) {
                vp64 = (uint64_t *)((byte *)stats->file->area + st->offset +
                                    (st->pos + st->count + date - st->current) % st->count *
                                    st->incr);
                STAT_UPD_VALUE(type, vp64 + index, vp64 + index_bis, value);
            }
        }
    }

    return 0;
}

int stats_temporal_upd(stats_temporal_t *stats, time_t date,
                       stats_upd_type type,
                       int index, int index_bis, int value)
{
    int status = 0;

    if (stats->do_sec) {
        if (stats_temporal_upd_sec(stats, date, type,
                                   index, index_bis, value))
            status = -1;
    }
    if (stats->do_hour) {
        if (stats_temporal_upd_hour(stats, date, type,
                                    index, index_bis, value))
            status = -1;
    }
    if (stats->nb_stages) {
        if (stats_temporal_upd_auto(stats, date, type,
                                    index, index_bis, value))
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

/* Should return number of samples produced? */
int stats_temporal_query_auto(stats_temporal_t *stats, blob_t *blob,
                              int start, int end, int nb_values,
                              bfield_t *mask, int fmt)
{
    static int pretty_freqs[] = {
        1, 2, 5, 10, 15, 20, 30,
        60, 120, 300, 600, 900, 1200, 1800,
        3600, 7200
    };
    static int nb_pretty_freqs = countof(pretty_freqs);
    int i, j, stage;
    stats_stage *st;
    int freq;
    int count = 0;
    int nb_stats = stats->nb_stats;
    double *accu;

    if (fmt < STATS_FMT_RAW || fmt > STATS_FMT_XML) {
        blob_append_fmt(blob, "Bad stats fmt: %d", fmt);
        return -1;
    }

    if (!stats->nb_stages) {
        blob_append_cstr(blob,
                         "stats auto deactivated for these statistics");
        return -1;
    }

    if (!stats->file) {
        stats->file = auto_file_initialize(stats, 0, false);
        if (!stats->file) {
            return -1;
        }
    }

    e_trace(1, "input values: start: %d, end: %d, nbvalues: %d",
            start, end, nb_values);
    if (start <= 0 || end <= 0) {
        blob_append_cstr(blob,
                         "Stats auto: invalid interval");
        return -1;
    }

    accu = p_new(double, nb_stats);

    /* Force minimum interval */
    if (nb_values < 10) {
        nb_values = 10;
    }
    if (end - start < 10) {
        int diff = 10 - (end - start);
        int rounding = diff & 0x1;  /* odd or even ? */

        diff  /= 2;
        start -= diff;
        end   += diff + rounding;
    }

    /* Compute best sample duration */
    freq = (end - start) / nb_values;
    e_trace(2, "freq requested: %d", freq);
    if (freq <= 1) {
        freq = 1;
    } else {
        for (i = 1; i < nb_pretty_freqs; i++) {
            if (freq < pretty_freqs[i]) {
                freq = pretty_freqs[i - 1];
                break;
            }
        }
        if (i == nb_pretty_freqs) {
            freq = -1;
        }
    }
    e_trace(2, "freq chosen: %d", freq);

    /* Choose stage */
    st = stats->file->area->stage;
    stage = 0;
    if (freq <= 0) {
        /* No freq was found (interval too large) */
        for (stage = 0; stage < stats->nb_stages - 1; stage++, st++) {
            if (((end - start) / st->scale) <= nb_values) {
                break;
            }
        }
        freq = st->scale;
    } else {
        if (st->scale < freq) {
            st++;
            for (stage = 1; stage < stats->nb_stages; stage++, st++) {
                if (st->scale > freq) {
                    /* Use the previous one */
                    st--;
                    stage--;
                    break;
                }
            }
            if (stage == stats->nb_stages) {
                /* Use the last as a last resort */
                stage--;
                st--;
            }
        } else {
            /* Use the first anyway */
        }
    }
    e_trace(2, "stage selected: %d", stage);

    start /= st->scale;
    freq  /= st->scale;
    start = (start / freq) * freq;
    end   = (end - 1) / st->scale + 1;

    e_trace(1, "real values chosen: start: %d, end: %d, freq: %d, "
            "nbvalues: %d, ratio: %d",
            start, end, freq, nb_values, (end - start) / freq);

    switch (fmt) {
      case STATS_FMT_XML:
        blob_append_cstr(blob, "<data>\n");
        break;

      default:
        break;
    }

    for (i = start, j = 0; i < end; i++) {
        int stageup = stage;
        stats_stage *stup = st;
        int stamp = i;

        if (i <= st->current + 1) {
            while (stamp <= stup->current - stup->count)
            {
                if (stageup >= stats->nb_stages - 1)
                    break;

                e_trace(2, "need next stage");
                stageup++;
                stup++;
                stamp = i * st->scale / stup->scale;
            }
            if (stamp <= stup->current - stup->count) {
                e_trace(2, "no data available, so dump 0");
            } else {
                double add;
                byte *vpup, *bufup_start, *bufup_end;

                bufup_start = (byte *)stats->file->area + stup->offset;
                bufup_end   = bufup_start + stup->count * stup->incr;
                vpup        = bufup_start + ((stup->pos + stup->count + stamp - stup->current) %
                                             stup->count * stup->incr);
                for (int k = 0; k < nb_stats; k++) {
                    if (!mask || bfield_isset(mask, k)) {
                        add = (double)(stageup ? ((int64_t*)vpup)[k] : ((int32_t*)vpup)[k]);
                        add = (add * st->scale) / stup->scale;
                        e_trace(3, "scaled add %e", add);
                        accu[k] += add;
                    }
                }
            }
        }
        j++;

        if (j >= freq) {
            switch (fmt) {
              case STATS_FMT_RAW:
                blob_append_fmt(blob, "%d",
                                (i - (freq - 1)) * st->scale);
                for (int k = 0; k < nb_stats; k++) {
                    if (!mask || bfield_isset(mask, k)) {
                        blob_append_fmt(blob, " %lld",
                                        (long long)accu[k] / (j * st->scale));
                    }
                }
                blob_append_cstr(blob, "\n");
                count++;
                break;

              case STATS_FMT_XML:
                {
                    bool first = true;
                    blob_append_fmt(blob, "<elem time=\"%d\" ",
                                    (i - (freq - 1)) * st->scale);
                    for (int k = 0; k < nb_stats; k++) {
                        if (!mask || bfield_isset(mask, k)) {
                            if (first) {
                                blob_append_fmt(blob, " val=\"%e\"",
                                                accu[k] / (j * st->scale));
                                first = false;
                            } else {
                                blob_append_fmt(blob, " val%d=\"%e\"", k,
                                                accu[k] / (j * st->scale));
                            }
                        }
                    }
                    blob_append_cstr(blob, "/>\n");
                }
                count++;
                break;

              default:
                break;
            }
            j = 0;
            p_clear(accu, nb_stats);
        }
    }


    switch (fmt) {
      case STATS_FMT_XML:
        blob_append_cstr(blob, "</data>\n");
        break;

      default:
        break;
    }

    p_delete(&accu);

    e_trace(1, "Outputed %d values", count);

    return count;
}

#ifndef NDEBUG
void stats_temporal_dump_auto(byte *mem, int size)
{
    stats_file_auto *file;
    int stage;

    if (size < ssizeof(stats_file_auto)) {
        e_trace(1, "mem buf too small");
        return;
    }

    file = (stats_file_auto *)mem;

    if (file->magic != STATS_AUTO_MAGIC) {
        e_trace(1, "magic incorrect");
        return;
    }

    printf("nb_stages: %d\n", file->nb_stages);
    printf("nb_allocated: %d\n", file->nb_allocated);
    printf("nb_stats: %d\n\n", file->nb_stats);

    for (stage = 0; stage < file->nb_stages; stage++) {
        stats_stage *st = &file->stage[stage];
        byte *buf_start, *buf_end, *vp;
        int i, index;

        printf("stage no: %d\n", stage);
        printf("scale: %d\n", st->scale);
        printf("count: %d\n", st->count);
        printf("current: %d\n", st->current);
        printf("pos: %d\n", st->pos);
        printf("incr: %d\n", st->incr);
        printf("offset: %d\n", st->offset);

        printf("\nStats:\n");

        buf_start = mem + st->offset;
        buf_end   = buf_start + st->count * st->incr;
        vp        = buf_start + ((st->pos + st->count - st->current) %
                                 st->count * st->incr);

        for (i = 0; i < st->count; i++) {
            if (vp >= buf_end) {
                vp = buf_start;
            }
            printf("%d", st->current + (i - st->pos));
            for (index = 0; index < file->nb_stats; index++) {
                printf(" %lld", (long long)(stage ? ((int64_t*)vp)[index] :
                    ((int32_t*)vp)[index]));
            }
            printf("\n");
            vp += st->incr;
        }
        printf("\n");
    }
}
#endif
