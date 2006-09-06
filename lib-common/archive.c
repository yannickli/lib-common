#include "archive.h"

#include <lib-common/mem.h>
#include <lib-common/err_report.h>

#define ARCHIVE_MAGIC 0x42

#define B4_TO_INT(b0, b1, b2, b3) \
         (((b0) << 24) +            \
          ((b1) << 16) +            \
          ((b2) << 8 ) +            \
          ((b3) << 0 ))
#define BYTESTAR_TO_INT(intput)   \
        B4_TO_INT(*(intput),      \
                  *(intput + 1),  \
                  *(intput + 2),  \
                  *(intput + 3))

static archive_t *archive_init(archive_t *archive)
{
    archive->payload = NULL;
    archive->blocs = NULL;
    archive->nb_blocs = 0;
    return archive;
}
static archive_t *archive_new(void)
{
    return archive_init(p_new(archive_t, 1));
}
static void archive_wipe(archive_t *archive)
{
    archive->payload = NULL;
    archive->blocs = NULL;
    archive->nb_blocs = 0;
}

void archive_delete(archive_t **archive)
{
    GENERIC_DELETE(archive_wipe, archive);
}

#define ARCHIVE_TAG_SIZE 4
#define ARCHIVE_SIZE_SIZE 4

static archive_head *archive_parse_head(const byte **input, int *len)
{
    input = input;
    len = len;
    /* TODO */
    return NULL;
}

static archive_file *archive_parse_file(const byte **input, int *len)
{
    input = input;
    len = len;
    /* TODO */
    return NULL;
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

archive_t *archive_parse(const byte *input, int len)
{
    archive_t *archive;
    bool dynamic = true;
    int allocated_blocs = 0;
    
    if (*input != ARCHIVE_MAGIC) {
        e_debug(1, "archive_parse: Bad magic number\n");
        return NULL;
    }

    archive = archive_new();

    input++;
    len--;
    archive->payload = input;

    if (len == 0) {
        return archive;
    }
    
    archive_head *head = archive_parse_head(&input, &len);
    if (head == NULL) {
        allocated_blocs = 8;
        archive->blocs = p_new(archive_bloc *, allocated_blocs);
        archive->nb_blocs = 0;
        dynamic = true;
    }
    else {
        archive->blocs = p_new(archive_bloc *, head->nb_blocs);
        archive->nb_blocs = 0;
        dynamic = true;
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
                /* If we read a number of blocs, we stop here
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
    
    return archive;
}


