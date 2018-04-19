/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
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

/* {{{ Private API */

#define FARCH_MAX_SYMBOL_SIZE  128

/** Farch archives data structures.
 *
 * XXX: You should never use these strutures directly: they are only exposed
 *      for farchc and the files it generates.
 *
 * Farch is used to store compressed file contents as C obfuscated symbols.
 *
 * File contents are obfuscated by splitting them in small chunks which are
 * xored with a randomly selected string (the string is a symbol which, in
 * principle, already exists in the binary). Each chunk is represented as a
 * farch_data_t structure.
 *
 * A farch archive is an array of farch_entry_t. Each of those represents a
 * single file with its obfuscated filename, its contents as described above,
 * and the size of the contents after and before compression. The obfuscation
 * method for the filename is the same than for the contents.
 */
typedef struct farch_data_t {
    int chunk_size; /* random length of the chunk */
    int xor_data_key; /* used to find the string for unobfuscation */
    const void * nonnull chunk; /* the obfuscated chunk of file contents */
} farch_data_t;

typedef struct farch_entry_t {
    lstr_t name; /* obfuscated filename */
    const farch_data_t * nonnull data; /* chunks of file contents */
    int compressed_size;
    int size;
    int nb_chunks;
    int xor_name_key; /* used to find the string for unobfuscation */
} farch_entry_t;

/** Obfuscate a string based on some internal data.
 *
 * This function should only be used by the farchc tool.
 */
void farch_obfuscate(const char * nonnull in, int len, int * nonnull xor_key,
                     char * nonnull out);

/* }}} */
/* {{{ Public API */

#define FARCH_MAX_FILENAME  (2 * PATH_MAX)

/** Get the filename of this entry.
 *
 * This function can be used to iterate on names entries: entries are stored
 * in an array terminating by an empty entry (name.len == 0).
 *
 *     for (const farch_entry_t *entry = entries,
 *              char __buf[FARCH_MAX_FILENAME];
 *          name = farch_get_filename(entry, __buf);
 *          entry++)
 *
 * \param[in]  file  a farch entry.
 * \param[in|out]  name  a buffer to write the name -- should contains
 *                       FARCH_MAX_FILENAME bytes.
 * \return  NULL if there is no entry or if an error occurs, name otherwise.
 */
char * nullable farch_get_filename(const farch_entry_t * nonnull file,
                                   char * nonnull name_outbuf);

/** Get the uncompressed content of a farch entry.
 *
 * Finds a farch entry by its name in a farch archive, and return its
 * uncompressed content.  If the name is not provided, the content of the
 * first entry is returned.
 *
 * \return  the uncompressed content if the entry is found,
 *          LSTR_NULL otherwise.
 */
lstr_t t_farch_get_data(const farch_entry_t * nonnull files,
                        const char * nullable name);

/** Similar to t_farch_get_uncompressed, but make the data persistent.
 *
 * The persisted data will be freed when the farch module is released.
 */
lstr_t farch_get_data_persist(const farch_entry_t * nonnull files,
                              const char * nullable name);

/* }}} */

/** Farch module.
 *
 * Using it is necessary only if \ref t_farch_uncompress_persist is used.
 */
MODULE_DECLARE(farch);

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif /* IS_LIB_COMMON_FARCH_H */
