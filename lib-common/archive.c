#include "archive.h"

#include <lib-common/mem.h>
#include <lib-common/err_report.h>

#define ARCHIVE_MAGIC 0x42

#define B4_TO_INT(b0, b1, b2, b3) \
         (((b0) << 24) +          \
          ((b1) << 16) +          \
          ((b2) << 8 ) +          \
          ((b3) << 0 ))
#define BYTESTAR_TO_INT(intput)   \
        B4_TO_INT(*(intput),      \
                  *(intput + 1),  \
                  *(intput + 2),  \
                  *(intput + 3))

#define UINT32_TO_B0(i) ((byte) (((i) >> 24) & 0x000000FF))
#define UINT32_TO_B1(i) ((byte) (((i) >> 16) & 0x000000FF))
#define UINT32_TO_B2(i) ((byte) (((i) >> 8 ) & 0x000000FF))
#define UINT32_TO_B3(i) ((byte) (((i) >> 0 ) & 0x000000FF))

#define ARCHIVE_MAGIC_SIZE 1
#define ARCHIVE_TAG_SIZE 4
#define ARCHIVE_SIZE_SIZE 4
#define ARCHIVE_VERSION_SIZE 4

#define ARCHIVE_TAG_FILE (B4_TO_INT('F', 'I', 'L', 'E'))
#define ARCHIVE_TAG_HEAD (B4_TO_INT('H', 'E', 'A', 'D'))
#define ARCHIVE_TAG_TPL  (B4_TO_INT('T', 'P', 'L', ' '))

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
    int i;

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


#define INCR_INPUT(nb)          \
    do {                        \
        int tmp = (nb);         \
        *input = *input + tmp;  \
        *len = *len - tmp;      \
    } while(0);
static archive_head *archive_parse_head(const byte **input, int *len)
{
    uint32_t tag;
    archive_head *head;

    /* +4 for number of blocs*/
    if (*len < ARCHIVE_TAG_SIZE + ARCHIVE_SIZE_SIZE + 4) {
        return NULL;
    }
    
    tag = BYTESTAR_TO_INT(*input);
    if (tag != B4_TO_INT('H', 'E', 'A', 'D')) {
        return NULL;
    }

    head = p_new(archive_head, 1);
    head->_bloc.tag   = tag;
    INCR_INPUT(4);

    head->_bloc.size  = BYTESTAR_TO_INT(*input);
    INCR_INPUT(4);

    head->nb_blocs = BYTESTAR_TO_INT(*input);
    INCR_INPUT(4);
    
    return head;;
}

static archive_file *archive_parse_file(const byte **input, int *len)
{
    uint32_t tag;
    archive_file *file;
    int i;
        

    /* + 3 * 4 + 1 : for 3 uint32 (size, creation, update)
     *               and 1 for at least one '\0'*/
    if (*len < ARCHIVE_TAG_SIZE + ARCHIVE_SIZE_SIZE + 3 * 4  + 1) {
        e_debug(2, "archive_parse_file: not enough header lenght: %d\n", *len);
         return NULL;
    }

    tag = BYTESTAR_TO_INT(*input);
    if (tag != B4_TO_INT('F', 'I', 'L', 'E')) {
        e_debug(2, "archive_parse_file: not a file tag: %u\n", tag);
        return NULL;
    }
    
    file = p_new(archive_file, 1);
    file->_bloc.tag   = tag;
    INCR_INPUT(4);

    file->_bloc.size  = BYTESTAR_TO_INT(*input);
    INCR_INPUT(4);

    file->size  = BYTESTAR_TO_INT(*input);
    INCR_INPUT(4);

    file->date_create  = BYTESTAR_TO_INT(*input);
    INCR_INPUT(4);

    file->date_update  = BYTESTAR_TO_INT(*input);
    INCR_INPUT(4);
    
    file->nb_attrs  = BYTESTAR_TO_INT(*input);
    INCR_INPUT(4);
        
    file->name = byte_to_char_const(*input);
    while ((*len) && (**input)) {
        INCR_INPUT(1);
    }
    if (*len == 0) {
        if (file->size == 0) {
            return file;
        }
        archive_file_delete(&file);
        e_debug(1, "archive_parse_file; Did not find \\0 while reading file name\n");
        return NULL;
    }

    INCR_INPUT(1);

    if (file->nb_attrs > 0) {
        file->attrs = p_new(archive_file_attr *, file->nb_attrs);
    }

    for(i = 0; i < file->nb_attrs; i++) {
        const char *key, *val;
        int key_len, val_len;

        key = byte_to_char_const(*input);
        key_len = 0;
        while ((*len) && (**input) && (**input != ':') && (**input != '\n')) {
            INCR_INPUT(1);
            key_len++;
        }
        if (**input != ':') {
            e_debug(1, "archive_parse_file; Missing : while reading file attr (key_len= %d)\n", key_len);
            if (i == 0) {
                p_delete(file->attrs);
                file->nb_attrs = 0;
            }
            else {
                file->attrs = p_renew(archive_file_attr *, file->attrs, file->nb_attrs, i);
                file->nb_attrs = i;
            }
            break;
        }
        INCR_INPUT(1);
        val = byte_to_char_const(*input);
        val_len = 0;

        while ((*len) && (**input) && (**input != '\n')) {
            INCR_INPUT(1);
            val_len++;
        }

        if (**input != '\n') {
            e_debug(1, "archive_parse_file; Missing \\n while reading file attr\n");
            if (i == 0) {
                p_delete(file->attrs);
                file->nb_attrs = 0;
            }
            else {
                file->attrs = p_renew(archive_file_attr *, file->attrs, file->nb_attrs, i);
                file->nb_attrs = i;
            }
            break;
        }

        file->attrs[i] = p_new(archive_file_attr, 1);
        file->attrs[i]->key     = key;
        file->attrs[i]->key_len = key_len;
        file->attrs[i]->val     = val;
        file->attrs[i]->val_len = val_len;

        INCR_INPUT(1);
    }

    file->payload = *input;

    if (((uint32_t) *len) < file->size) {
        e_debug(1, "archive_parse_file; Not enough len remaining\n");
        archive_file_delete(&file);
        return NULL;
    }
    INCR_INPUT(file->size);
    
    return file;
}

static archive_tpl *archive_parse_tpl(const byte **input, int *len)
{
    input = input;
    len = len;
    /* TODO */
    return NULL;
}

static archive_bloc *archive_parse_bloc(const byte **input, int *len)
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
          e_debug(1, "archive_parse_bloc: unrecognized bloc tag: %u", tag);
          return NULL;
    }
}

int archive_parse(const byte *input, int len, archive_t *archive)
{
    bool dynamic = true;
    int allocated_blocs = 0;
    uint32_t version;
    
    if (*input != ARCHIVE_MAGIC) {
        e_debug(1, "archive_parse: Bad magic number\n");
        return 1;
    }

    input += ARCHIVE_MAGIC_SIZE;
    len -= ARCHIVE_MAGIC_SIZE;

    if (len < ARCHIVE_VERSION_SIZE) {
        e_debug(1, "archive_parse: Not enough length to read version\n");
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
        archive->blocs = p_new(archive_bloc *, head->nb_blocs + 1);
        archive->nb_blocs = 1;
        archive->blocs[0] = archive_head_to_archive_bloc(head);
        dynamic = false;
    }

    while (len > 0) {
        archive_bloc *bloc = archive_parse_bloc(&input, &len);
        if (bloc == NULL) {
            break;
        }
        archive->blocs[archive->nb_blocs] = bloc;
        archive->nb_blocs++;

        if (archive->nb_blocs >= allocated_blocs) {
            if (!dynamic) {
                /* If we read a greater number of blocs, we stop here
                 *  FIXME: should we try to read more blocs anyway ?
                 * */
                break;
            }
            archive->blocs = p_renew(archive_bloc *, archive->blocs, allocated_blocs,
                                                     2 * allocated_blocs);
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

#ifdef CHECK

#include "blob.h"
#include <stdio.h>

START_TEST(check_init_wipe)
{
    archive_t archive;
    archive_init(&archive);

    fail_if(archive.nb_blocs != 0, "initalized archive MUST have `nb_blocs' = 0, "
                                   "but has `nb_blocs = %d'", archive.nb_blocs);
    fail_if(archive.blocs != NULL,  "initalized archive MUST have a valid `blocs'");
    fail_if(archive.payload != NULL,  "initalized payload MUST have a valid `payload'");

    archive_wipe(&archive);
    fail_if(archive.blocs != NULL,   "wiped blob MUST have `blocs' set to NULL");
    fail_if(archive.payload != NULL, "wiped blob MUST have `payload' set to NULL");
}
END_TEST

START_TEST(check_parse)
{
#define AR_APPEND_UINT32(blob, val)                           \
    do {                                                \
    i = (val);                                          \
    blob_append_byte((blob), UINT32_TO_B0(i));  \
    blob_append_byte((blob), UINT32_TO_B1(i));  \
    blob_append_byte((blob), UINT32_TO_B2(i));  \
    blob_append_byte((blob), UINT32_TO_B3(i));  \
    } while (0);

    archive_t archive;
    archive_init(&archive);
    uint32_t i;
    int res;
    blob_t file;
    blob_t parse_payload;

    blob_init(&parse_payload);
    blob_append_byte(&parse_payload, ARCHIVE_MAGIC);
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
    AR_APPEND_UINT32(&parse_payload, file.len);/* Bloc size (4 for one uint32)*/
    blob_append(&parse_payload, &file);

    res = archive_parse(parse_payload.data, parse_payload.len, &archive);

    fail_if(res != 0,  "archive_parse failed on a valid archive");
    fail_if(archive.version != 1,  "archive_parse failed on a reading version");
    fail_if(archive.nb_blocs != 2,  "archive_parse failed to read correct number of blocs");

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
