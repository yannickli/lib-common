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

#include "archive.h"

#include <lib-common/mem.h>
#include <lib-common/err_report.h>
#include <lib-common/archive_priv.h>

archive_t *archive_new(void)
{
    return archive_init(p_new(archive_t, 1));
}
archive_t *archive_init(archive_t *archive)
{
    archive->version = 0;
    archive->payload = NULL;
    archive->blocs = NULL;
    archive->nb_blocs = 0;
    archive->last_bloc = NULL;
    return archive;
}

static void archive_file_delete(archive_file **file);
static void archive_head_delete(archive_head **head);
static void archive_tpl_delete(archive_tpl **tpl);
void archive_wipe(archive_t *archive)
{
    int i;

    archive->payload = NULL;
    if (archive->nb_blocs) {
        for (i = 0; i < archive->nb_blocs ; i++) {
            archive_bloc *bloc = archive->blocs[i];
            switch (bloc->tag) {
              case ARCHIVE_TAG_FILE:
                archive_file_delete(archive_bloc_to_archive_file_p(&bloc));
                break;
              case ARCHIVE_TAG_TPL:
                archive_tpl_delete(archive_bloc_to_archive_tpl_p(&bloc));
                break;
              case ARCHIVE_TAG_HEAD:
                archive_head_delete(archive_bloc_to_archive_head_p(&bloc));
                break;
              default:
                p_delete(&bloc);
                break;
            }
        }
        p_delete(&archive->blocs);
    }
    archive->nb_blocs = 0;
    archive->version = 0;
    archive->last_bloc = NULL;
}

void archive_delete(archive_t **archive)
{
    GENERIC_DELETE(archive_wipe, archive);
}

static inline const char *byte_to_char_const(const byte *b)
{
    return (const char *) b;
}

static void archive_bloc_wipe(archive_bloc *bloc)
{
    bloc->tag  = 0;
    bloc->size = 0;
    bloc->payload = NULL;
}

static void archive_file_wipe(archive_file *file)
{
    uint32_t i;

    archive_bloc_wipe(&file->_bloc);
    file->size = 0;
    file->date_create = 0;
    file->date_update = 0;
    file->name = NULL;
    if (file->nb_attrs) {
        for (i = 0; i < file->nb_attrs; i++) {
            p_delete(&(file->attrs[i]));
        }
        p_delete(&file->attrs);
        file->nb_attrs = 0;
    }
    file->payload = NULL;
}
static void archive_head_wipe(archive_head *head)
{
    archive_bloc_wipe(&head->_bloc);
    head->nb_blocs = 0;
}
static void archive_tpl_wipe(archive_tpl *tpl)
{
    archive_bloc_wipe(&tpl->_bloc);
}
static void archive_file_delete(archive_file **file)
{
    GENERIC_DELETE(archive_file_wipe, file);
}
static void archive_head_delete(archive_head **head)
{
    GENERIC_DELETE(archive_head_wipe, head);
}
static void archive_tpl_delete(archive_tpl **tpl)
{
    GENERIC_DELETE(archive_tpl_wipe, tpl);
}


static inline int read_uint32(const byte **input, int *len, uint32_t *val)
{
    if (*len < 4) {
        return -1;
    }
    if (val) {
        *val = BYTESTAR_TO_INT(*input);
    }
    (*input) += 4;
    (*len) -= 4;
    return 0;
}

static inline int read_const_char(const byte **input, int *len,
                                  const char **val)
{
    const byte *old_input = *input;
    int old_len = *len;

    while ((*len) && (**input)) {
        (*input)++;
        (*len)--;
    }
    (*input)++;
    (*len)--;
    if (*len < 0 || (*len == 0 && !**input)) {
        *input = old_input;
        *len = old_len;
        return -1;
    }
    *val = byte_to_char_const(old_input);
    return 0;
}

static archive_head *archive_parse_head(const byte **input, int *len)
{
    uint32_t tag;
    archive_head *head = NULL;
    const byte *old_input;
    int old_len;

    if (!input || !len) {
        return NULL;
    }
    old_input = *input;
    old_len = *len;

    if (read_uint32(input, len, &tag)) {
        goto error;
    }
    if (tag != B4_TO_INT('H', 'E', 'A', 'D')) {
        goto error;
    }

    head = p_new(archive_head, 1);
    head->_bloc.tag   = tag;

    if (read_uint32(input, len, &head->_bloc.size)) {
        goto error;
    }

    if (read_uint32(input, len, &head->nb_blocs)) {
        p_delete (&head);
        return NULL;
    }

    return head;

error:
    p_delete (&head);
    *len = old_len;
    *input = old_input;
    return NULL;
}

static archive_file *archive_parse_file(const byte **input, int *len)
{
    uint32_t tag;
    archive_file *file = NULL;
    uint32_t i;
    const byte *old_input;
    int old_len;

    if (!input || !len) {
        return NULL;
    }
    old_input = *input;
    old_len = *len;

    if (read_uint32(input, len, &tag)) {
        goto error;
    }
    if (tag != B4_TO_INT('F', 'I', 'L', 'E')) {
        e_debug(2, E_PREFIX("not a file tag: %u\n"), tag);
        goto error;
    }

    file = p_new(archive_file, 1);
    file->_bloc.tag   = tag;

    if (read_uint32(input, len, &file->_bloc.size)
    ||  read_uint32(input, len, &file->size)
    ||  read_uint32(input, len, &file->date_create)
    ||  read_uint32(input, len, &file->date_update)
    ||  read_uint32(input, len, &file->nb_attrs)
    ) {
        goto error;
    }

    if (read_const_char(input, len, &file->name)) {
        e_debug(1, E_PREFIX("Did not find \\0 while reading file name\n"));
        goto error;
    }
    if (*len == 0) {
        if (file->size == 0 && file->nb_attrs == 0) {
            return file;
        }
        goto error;
    }

    if (file->nb_attrs > 0) {
        file->attrs = p_new(archive_file_attr *, file->nb_attrs);
    }

    /* XXX: This loop needs bounds checking. */
    for(i = 0; i < file->nb_attrs; i++) {
        const char *key, *val;
        int key_len, val_len;

        key = byte_to_char_const(*input);
        key_len = 0;
        while ((*len) && (**input) && (**input != ':') && (**input != '\n')) {
            (*input)++;
            (*len)--;
            key_len++;
        }
        if (**input != ':') {
            e_debug(1, E_PREFIX("Missing :"
                       "while reading file attr (key_len= %d)\n"), key_len);
            if (i == 0) {
                p_delete(file->attrs);
                file->nb_attrs = 0;
            }
            else {
                file->attrs = p_renew(archive_file_attr *, file->attrs,
                                      file->nb_attrs, i);
                file->nb_attrs = i;
            }
            break;
        }
        (*input)++;
        (*len)--;
        val = byte_to_char_const(*input);
        val_len = 0;

        while ((*len) && (**input) && (**input != '\n')) {
            (*input)++;
            (*len)--;
            val_len++;
        }

        if (**input != '\n') {
            e_debug(1, E_PREFIX("Missing \\n while reading file attr\n"));
            if (i == 0) {
                p_delete(file->attrs);
                file->nb_attrs = 0;
            }
            else {
                file->attrs = p_renew(archive_file_attr *, file->attrs,
                                      file->nb_attrs, i);
                file->nb_attrs = i;
            }
            break;
        }

        file->attrs[i] = p_new(archive_file_attr, 1);
        file->attrs[i]->key     = key;
        file->attrs[i]->key_len = key_len;
        file->attrs[i]->val     = val;
        file->attrs[i]->val_len = val_len;

        (*input)++;
        (*len)--;
    }

    file->payload = *input;

    if (((uint32_t) *len) < file->size) {
        e_debug(1, E_PREFIX("Not enough len remaining\n"));
        goto error;
    }
    (*input) += file->size;
    (*len) -= file->size;

    return file;
error:
    archive_file_delete(&file);
    *len = old_len;
    *input = old_input;
    return NULL;
}

static archive_tpl *archive_parse_tpl(const byte **input, int *len)
{
    input = input;
    len = len;
    /* TODO */
    return NULL;
}

archive_bloc *archive_parse_bloc(const byte **input, int *len);

archive_bloc *archive_parse_bloc(const byte **input, int *len)
{
    uint32_t tag;

    if (*len < ARCHIVE_TAG_SIZE + ARCHIVE_SIZE_SIZE) {
        return NULL;
    }

    tag  = BYTESTAR_TO_INT(*input);

    switch (tag) {
        case B4_TO_INT('F', 'I', 'L', 'E'):
          return archive_file_to_archive_bloc(archive_parse_file(input, len));
          break;
        case B4_TO_INT('T', 'P', 'L', ' '):
          return archive_tpl_to_archive_bloc(archive_parse_tpl(input, len));
          break;
        case B4_TO_INT('H', 'E', 'A', 'D'):
          return archive_head_to_archive_bloc(archive_parse_head(input, len));
          break;
        default:
          e_debug(1, E_PREFIX("unrecognized bloc tag: %u\n"), tag);
          return NULL;
    }
}

int archive_parse(const byte *input, int len, archive_t *archive)
{
    bool dynamic = true;
    int allocated_blocs = 0;
    uint32_t version;

    if (input[0] != ARCHIVE_MAGIC0
    ||  input[1] != ARCHIVE_MAGIC1
    ||  input[2] != ARCHIVE_MAGIC2
    ||  input[3] != ARCHIVE_MAGIC3) {
        e_debug(1, E_PREFIX("Bad magic number\n"));
        return 1;
    }

    input += ARCHIVE_MAGIC_SIZE;
    len -= ARCHIVE_MAGIC_SIZE;

    if (len < ARCHIVE_VERSION_SIZE) {
        e_debug(1, E_PREFIX("Not enough length to read version\n"));
        return 1;
    }
    version = BYTESTAR_TO_INT(input);

    input += ARCHIVE_VERSION_SIZE;
    len -= ARCHIVE_VERSION_SIZE;

    archive->version = version;

    archive->payload = input;


    if (len == 0) {
        return 0;
    }

    archive_head *head = archive_parse_head(&input, &len);
    if (head == NULL) {
        allocated_blocs = 8;
        archive->blocs = p_new(archive_bloc *, allocated_blocs);
        archive->nb_blocs = 0;
        dynamic = true;
    }
    else {
        allocated_blocs = head->nb_blocs + 1;
        archive->blocs = p_new(archive_bloc *, allocated_blocs);
        archive->blocs[0] = archive_head_to_archive_bloc(head);
        archive->nb_blocs = 1;
        dynamic = false;
    }

    while (len > 0) {
        archive_bloc *bloc = archive_parse_bloc(&input, &len);
        if (bloc == NULL) {
            break;
        }
        archive->blocs[archive->nb_blocs] = bloc;
        archive->nb_blocs++;
        archive->last_bloc = bloc;

        if (archive->nb_blocs >= allocated_blocs) {
            if (!dynamic) {
                /* If we read a greater number of blocs, we stop here
                 *  FIXME: should we try to read more blocs anyway ?
                 * */
                break;
            }
            archive->blocs = p_renew(archive_bloc *, archive->blocs,
                                     allocated_blocs, 2 * allocated_blocs);
            allocated_blocs *= 2;
        }
    }

    if (dynamic) {
        if (allocated_blocs > archive->nb_blocs) {
            archive->blocs = p_renew(archive_bloc *, archive->blocs,
                                     allocated_blocs, archive->nb_blocs);
        }
    }

    return 0;
}

const archive_file *archive_get_file_bloc(const archive_t *archive,
                                          const char *filename)
{
    archive_bloc *bloc;
    archive_file *file;
    int i;

    for (i = 0; i < archive->nb_blocs; i++) {
        bloc = archive->blocs[i];
        if (bloc->tag == ARCHIVE_TAG_FILE) {
            file = archive_bloc_to_archive_file(bloc);

            if (!strcmp(file->name, filename)) {
                return file;
            }
        }
    }

    return NULL;
}

int archive_parts_in_path(const archive_t *archive, const char *path)
{
    archive_bloc *bloc;
    archive_file *file;
    int i, res = 0;

    for (i = 0; i < archive->nb_blocs; i++) {
        bloc = archive->blocs[i];
        if (bloc->tag == ARCHIVE_TAG_FILE) {
            file = archive_bloc_to_archive_file(bloc);

            res += strstart(file->name, path, NULL);
        }
    }
    return res;
}


const archive_file *archive_file_next(const archive_t *archive,
                                      const archive_file* previous)
{
    int i;
    const archive_bloc *previous_b =
                    archive_file_to_archive_bloc_const(previous);

    if (!archive->blocs) {
        return NULL;
    }
    if (previous_b) {
        if (previous_b == archive->last_bloc) {
            return NULL;
        }
    } else {
        previous_b = archive->blocs[0];
    }

    /* find previous in the list of blocs */
    for (i = 0; i < archive->nb_blocs + 1; i++) {
        if (archive->blocs[i] == previous_b) {
            break;
        }
    }
    /* Examine the next one */
    i++;
    /* find the next file */
    for (;i < archive->nb_blocs + 1; i++) {
        if (archive->blocs[i]->tag == ARCHIVE_TAG_FILE) {
            break;
        }
    }
    if (i < archive->nb_blocs + 1
    && archive->blocs[i]->tag == ARCHIVE_TAG_FILE) {
        return archive_bloc_to_archive_file(archive->blocs[i]);
    }
    return NULL;
}

const archive_file *archive_file_next_path(const archive_t *archive,
                                           const char *path,
                                           const archive_file* previous)
{
    const archive_file *file;

    file = archive_file_next(archive, previous);

    while (file) {
        if (strstart(file->name, path, NULL)) {
            return file;
        }
        file = archive_file_next(archive, file);
    }
    return NULL;
}

static void archive_file_dump(const archive_file *file, int level)
{
    e_debug(level, E_PREFIX("archive_file :\n"));
    e_debug(level, E_PREFIX(" - size = %d\n"),
            file->size);
    e_debug(level, E_PREFIX(" - date_create = %d\n"),
            file->date_create);
    e_debug(level, E_PREFIX(" - date_update = %d\n"),
            file->date_update);
    e_debug(level, E_PREFIX(" - name = %s\n"),
            file->name);
    e_debug(level, E_PREFIX(" - nb_attrs = %d\n"),
            file->nb_attrs);
    e_debug(level, E_PREFIX(" - payload = %.*s\n"),
            file->size, file->payload);
}
static void archive_head_dump(const archive_head *head, int level)
{
    e_debug(level, E_PREFIX("archive_head :\n"));
    e_debug(level, E_PREFIX(" - nb_blocs = %d\n"),
            head->nb_blocs);
}
static void archive_tpl_dump(const archive_tpl __unused__ *tpl, int level)
{
    e_debug(level, E_PREFIX("NOT IMPLEMENTED !\n"));
}

void archive_dump(const archive_t *archive, int level)
{
    int i;

    e_debug(level, E_PREFIX("archive :\n - version = %d\n"),
            archive->version);
    e_debug(level, E_PREFIX(" - nb_blocs = %d\n"),
            archive->nb_blocs);
    
    if (archive->nb_blocs) {
        for (i = 0; i < archive->nb_blocs ; i++) {
            archive_bloc *bloc = archive->blocs[i];
            switch (bloc->tag) {
              case ARCHIVE_TAG_FILE:
                archive_file_dump(archive_bloc_to_archive_file(bloc), level);
                break;
              case ARCHIVE_TAG_TPL:
                archive_tpl_dump(archive_bloc_to_archive_tpl(bloc), level);
                break;
              case ARCHIVE_TAG_HEAD:
                archive_head_dump(archive_bloc_to_archive_head(bloc), level);
                break;
              default:
                e_debug(level, "archive_bloc : unknown bloc type!\n");
                break;
            }
        }
    }

}

#ifdef CHECK

#include "blob.h"
#include <stdio.h>

START_TEST(check_init_wipe)
{
    archive_t archive;
    archive_init(&archive);

    fail_if(archive.nb_blocs != 0,
            "initalized archive MUST have `nb_blocs' = 0, "
            "but has `nb_blocs = %d'", archive.nb_blocs);
    fail_if(archive.blocs != NULL,
            "initalized archive MUST have a valid `blocs'");
    fail_if(archive.payload != NULL,
            "initalized payload MUST have a valid `payload'");

    archive_wipe(&archive);
    fail_if(archive.blocs != NULL,
            "wiped blob MUST have `blocs' set to NULL");
    fail_if(archive.payload != NULL,
            "wiped blob MUST have `payload' set to NULL");
}
END_TEST

START_TEST(check_parse)
{
#define AR_APPEND_UINT32(blob, val)                     \
    do {                                                \
    i = (val);                                          \
    blob_append_byte((blob), UINT32_TO_B0(i));          \
    blob_append_byte((blob), UINT32_TO_B1(i));          \
    blob_append_byte((blob), UINT32_TO_B2(i));          \
    blob_append_byte((blob), UINT32_TO_B3(i));          \
    } while (0);

    archive_t archive;
    archive_init(&archive);
    uint32_t i;
    int res;
    blob_t file;
    blob_t parse_payload;

    blob_init(&parse_payload);
    blob_append_byte(&parse_payload, ARCHIVE_MAGIC0);
    blob_append_byte(&parse_payload, ARCHIVE_MAGIC1);
    blob_append_byte(&parse_payload, ARCHIVE_MAGIC2);
    blob_append_byte(&parse_payload, ARCHIVE_MAGIC3);
    AR_APPEND_UINT32(&parse_payload, 1);/* Version */

    /* HEAD */
    AR_APPEND_UINT32(&parse_payload, B4_TO_INT('H', 'E', 'A', 'D'));
    AR_APPEND_UINT32(&parse_payload, 4);/* Bloc size (4 for one uint32)*/
    AR_APPEND_UINT32(&parse_payload, 1);/* Number of blocs*/

    /* FILE */
    blob_init(&file);
    const char *file_data = "Test data\n";
    AR_APPEND_UINT32(&file, strlen(file_data));/* File size */
    AR_APPEND_UINT32(&file, 0);/* Date crea */
    AR_APPEND_UINT32(&file, 0);/* Date update */
    AR_APPEND_UINT32(&file, 2);/* attrs number */
    blob_append_cstr(&file, "test.fic");/* Name */
    blob_append_byte(&file, 0);
    blob_append_cstr(&file, "Key1:val1\n");
    blob_append_cstr(&file, "Key2:val2\n");
    blob_append_cstr(&file, file_data);


    AR_APPEND_UINT32(&parse_payload, B4_TO_INT('F', 'I', 'L', 'E'));
    /* Bloc size (4 for one uint32)*/
    AR_APPEND_UINT32(&parse_payload, file.len);
    blob_append(&parse_payload, &file);

    res = archive_parse(parse_payload.data, parse_payload.len, &archive);

    fail_if(res != 0,
            "archive_parse failed on a valid archive");
    fail_if(archive.version != 1,
            "archive_parse failed on a reading version");
    fail_if(archive.nb_blocs != 2,
            "archive_parse failed to read correct number of blocs");

    archive_wipe(&archive);
    blob_wipe(&file);
    blob_wipe(&parse_payload);
}
END_TEST

Suite *check_make_archive_suite(void)
{
    Suite *s  = suite_create("Archive");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_init_wipe);
    tcase_add_test(tc, check_parse);

    return s;
}

#endif
