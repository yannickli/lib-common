/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_FARCH_H
#define IS_LIB_COMMON_FARCH_H

#include "core.h"

#if __has_feature(nullability)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wnullability-completeness"
#if __has_warning("-Wnullability-completeness-on-arrays")
#pragma GCC diagnostic ignored "-Wnullability-completeness-on-arrays"
#endif
#endif

typedef struct farch_entry_t {
    const char * nonnull name;
    const void * nonnull data;
    int compressed_size;
    int size;
} farch_entry_t;

/** Get compressed farch entry.
 *
 * Get a farch entry by its name in a farch archive.
 *
 * \return  a pointer on the farch entry (compressed) if found,
 *          NULL otherwise.
 */
const farch_entry_t * nullable farch_get_entry(const farch_entry_t files[],
                                               const char * nonnull name);

/** Get the uncompressed content of a farch entry.
 *
 * Finds a farch entry by its name in a farch archive, and return its
 * uncompressed content.
 *
 * \return  the uncompressed content if the entry is found,
 *          LSTR_NULL otherwise.
 */
lstr_t t_farch_get_uncompressed(const farch_entry_t files[],
                                const char * nonnull name);

/** Uncompress a farch entry. */
lstr_t t_farch_uncompress(const farch_entry_t * nonnull entry);

/** Uncompress a farch entry, and make the data persistent.
 *
 * The persisted data will be freed when the farch module is released.
 */
lstr_t farch_uncompress_persist(const farch_entry_t * nonnull entry);


/** Farch module.
 *
 * Using it is necessary only if \ref t_farch_uncompress_persist is used.
 */
MODULE_DECLARE(farch);

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif /* IS_LIB_COMMON_FARCH_H */
