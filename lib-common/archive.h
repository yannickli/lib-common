#ifndef __LIB_COMMON_ARCHIVE_H__
#define __LIB_COMMON_ARCHIVE_H__

#include <inttypes.h>
#include "macros.h"

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
    int nb_attrs;
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
    const byte *payload;
    archive_bloc **blocs;
    int nb_blocs;
} archive_t;

CONVERSION_FUNCTIONS(archive_head, archive_bloc);
CONVERSION_FUNCTIONS(archive_file, archive_bloc);
CONVERSION_FUNCTIONS(archive_tpl, archive_bloc);

/**
 *  FIXME: should len be greater than "int" ?
 */
int archive_parse(const byte *input, int len, archive_t *archive);

archive_t *archive_new(void);
archive_t *archive_init(archive_t *archive);
void archive_wipe(archive_t *archive);
void archive_delete(archive_t **archive);

#ifdef CHECK
#include <check.h>

Suite *check_make_archive_suite(void);

#endif


#endif
