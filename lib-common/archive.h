#ifndef __LIB_COMMON_ARCHIVE_H__
#define __LIB_COMMON_ARCHIVE_H__

typedef struct {
    unsigned int32_t tag;
    unsigned int32_t size;
    const byte *payload;
} archive_bloc;

typedef struct {
    archive_bloc _bloc;
    unsigned int32_t size;
    unsigned int32_t date_create;
    unsigned int32_t date_update;
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
    unsigned int32_t nb_blocs;
} archive_head;

typedef struct {
    const byte *payload;
    archive_bloc **blocs;
    int nb_blocs;
} archive_t;

CONVERSION_FUNCTIONS(archive_file, archive_bloc);
CONVERSION_FUNCTIONS(archive_tpl, archive_bloc);

/**
 *  FIXME: should len be grater than "int" ?
 */
archive_t *archive_parse(const byte *input, int len);

void archive_delete(archive_t **archive);

#endif
