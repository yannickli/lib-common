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

#ifndef IS_LIB_COMMON_ARCHIVE_H
#define IS_LIB_COMMON_ARCHIVE_H

#include <inttypes.h>

#include <lib-common/mem.h>

#include "macros.h"

/**
 *  Archive format :
 *
 *
 */

typedef struct {
    uint32_t tag;
    uint32_t size;
    const byte *payload;
} archive_bloc;

typedef struct {
    const char *key;
    int key_len;
    const char *val;
    int val_len;
} archive_file_attr;

typedef struct {
    archive_bloc _bloc;
    uint32_t size;
    uint32_t date_create;
    uint32_t date_update;
    const char *name;
    archive_file_attr **attrs;
    uint32_t nb_attrs;
    const byte* payload;
} archive_file;

typedef struct {
    archive_bloc _bloc;
    /** TODO */
} archive_tpl;

typedef struct {
    archive_bloc _bloc;
    uint32_t nb_blocs;
} archive_head;

typedef struct {
    uint32_t version;
    const byte *payload;
    archive_bloc **blocs;
    int nb_blocs;
    archive_bloc *last_bloc;
} archive_t;

CONVERSION_FUNCTIONS(archive_head, archive_bloc);
CONVERSION_FUNCTIONS(archive_file, archive_bloc);
CONVERSION_FUNCTIONS(archive_tpl, archive_bloc);

/**
 *  FIXME: should len be greater than "int" ?
 */
int archive_parse(const byte *input, int len, archive_t *archive);

archive_t *archive_init(archive_t *archive);
GENERIC_NEW(archive_t, archive);
void archive_wipe(archive_t *archive);
GENERIC_DELETE(archive_t, archive);

const archive_file *archive_get_file_bloc(const archive_t *archive,
                                          const char *filename);
int archive_parts_in_path(const archive_t *archive,
                          const char *path);
const archive_file *archive_file_next(const archive_t *archive,
                                      const archive_file* previous);
const archive_file *archive_file_next_path(const archive_t *archive,
                                           const char *path,
                                           const archive_file* previous);

void archive_dump(const archive_t *archive, int level);

#ifdef CHECK
#include <check.h>

Suite *check_make_archive_suite(void);

#endif
#endif /* IS_LIB_COMMON_ARCHIVE_H */
