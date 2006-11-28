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

#include "macros.h"
#include "mem.h"
#include "blob.h"
#include "array.h"

/*
 * Archive format :
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
    const byte *payload;
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

GENERIC_INIT(archive_t, archive);
GENERIC_NEW(archive_t, archive);
void archive_wipe(archive_t *archive);
GENERIC_DELETE(archive_t, archive);

const archive_file *archive_get_file_bloc(const archive_t *archive,
                                          const char *filename);
int archive_parts_in_path(const archive_t *archive,
                          const char *path);
const archive_file *archive_file_next(const archive_t *archive,
                                      const archive_file *previous);
const archive_file *archive_file_next_path(const archive_t *archive,
                                           const char *path,
                                           const archive_file *previous);

#ifdef NDEBUG
#  define archive_dump(...)
#else
void archive_dump(const archive_t *archive, int level);
#endif

/****************************************************************************/
/* Archive building types                                                   */
/****************************************************************************/

typedef struct archive_attr {
    char *key;
    char *val;
} archive_attr;

ARRAY_TYPE(archive_attr, archive_attr);

static inline void archive_attr_wipe(archive_attr *attr) {
    p_delete(&attr->key);
    p_delete(&attr->val);
}

GENERIC_DELETE(archive_attr, archive_attr);
ARRAY_FUNCTIONS(archive_attr, archive_attr);

typedef struct archive_build {
    char *name;

    uint32_t date_create;
    uint32_t date_update;
    archive_attr_array attrs;

    byte *payload;
    int payload_len;
} archive_build;

ARRAY_TYPE(archive_build, archive_build);

void archive_build_wipe(archive_build *file);
GENERIC_DELETE(archive_build, archive_build);
ARRAY_FUNCTIONS(archive_build, archive_build);


void archive_add_property(archive_build *file,
                          const char *name, const char *value);

archive_build *
archive_add_file(archive_build_array *arch,
                 const char *name, const byte *payload, int len);
static inline archive_build *
archive_add_cstr(archive_build_array *arch, const char *name, const char *s) {
    return archive_add_file(arch, name, (const byte *)s, strlen(s));
}
archive_build *archive_find_file(archive_build_array *, const char *name);

int blob_append_archive(blob_t *output, const archive_build_array *archive);


#ifdef CHECK
#include <check.h>

Suite *check_make_archive_suite(void);

#endif
#endif /* IS_LIB_COMMON_ARCHIVE_H */
