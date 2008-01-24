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

#ifndef IS_LIB_COMMON_STATS_TEMPORAL_H
#define IS_LIB_COMMON_STATS_TEMPORAL_H

#include <lib-common/macros.h>
#include <lib-common/bfield.h>
#include <lib-common/blob.h>
#include <lib-common/array.h>

#define STATS_STAGE_MAX          7
#define STATS_TEMPORAL_AUTO(n)   ((n) << 2)
#define STATS_TEMPORAL_SECONDS   (1 << 0)
#define STATS_TEMPORAL_HOURS     (1 << 1)

#define STATS_FMT_RAW  1
#define STATS_FMT_XML  2

typedef enum stats_upd_type {
    STATS_UPD_INCR,
    STATS_UPD_MIN,
    STATS_UPD_MAX,
    STATS_UPD_MEAN,
} stats_upd_type;

typedef struct stats_temporal_t stats_temporal_t;

/* flags is a combination of STATS_TEMPORAL_SECONDS and
 * STATS_TEMPORAL_HOURS or STATS_TEMPORAL_AUTO(n) where n is the number
 * of stages of automatic aggregation
 * desc is an array of aggregation steps and counts
 */
stats_temporal_t *stats_temporal_new(const char *path, int nb_stats,
                                     int flags, const int *desc);
void stats_temporal_delete(stats_temporal_t **stats);
ARRAY_TYPE(stats_temporal_t, stats_temporal);
ARRAY_FUNCTIONS(stats_temporal_t, stats_temporal, stats_temporal_delete);

/* index_bis is currently only used for type = STATS_UPD_MEAN to indicate in which
 * index is stored the total number of samples */
int stats_temporal_upd(stats_temporal_t * stats, time_t date,
                       stats_upd_type type,
                       int index, int index_bis,
                       int value);
bool stats_temporal_shrink(stats_temporal_t *stats, int date);

/* Query */
__must_check__
int stats_temporal_query_sec(stats_temporal_t *stats, blob_t *blob,
                             int *outp, int start, int nb_values,
                             bfield_t *mask);
__must_check__
int stats_temporal_query_hour(stats_temporal_t *stats, blob_t *blob,
                              int64_t *outp, int start, int nb_values,
                              bfield_t *mask);
__must_check__
int stats_temporal_query_auto(stats_temporal_t *stats, blob_t *blob,
                              int start, int end, int nb_values,
                              bfield_t *mask, int fmt);

#ifndef NDEBUG
void stats_temporal_dump_auto(byte *mem, int size);
#else
#define stats_temporal_dump_auto(...)
#endif


#endif /* IS_LIB_COMMON_STATS_TEMPORAL_H */
