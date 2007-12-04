/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "file-binlog.h"
#include "hash.h"

struct binlog_hdr_t {
    be16_t kind;
    be16_t len;
};

struct binlog_pad_t {
    uint8_t pad[4];
    be32_t crc;
};

struct binlog_t *binlog_wopen(const char *name)
{
    binlog_t res = { .writing = 1 };
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL);

    if (fd < 0)
        return NULL;

    res.fp = fdopen(fd, "a+");
    if (!res.fp)
        return NULL;

    res.fsize = ftell(res.fp);
    if (res.fsize < 4)
        goto error;

    fseek(res.fp, -4, SEEK_CUR);
    while (fread(&res.crc, 4, 1, res.fp) < 1) {
        if (errno != EAGAIN && errno != EINTR)
            goto error;
    }

    return p_dup(&res, 1);

 error:
    p_fclose(&res.fp);
    return NULL;
}

struct binlog_t *binlog_create(const char *name)
{
    binlog_t res = { .writing = 1 };
    int fd = open(name, O_RDWR | O_CREAT | O_EXCL);

    if (fd < 0)
        return NULL;

    res.fp = fdopen(fd, "a+");
    if (!res.fp) {
        close(fd);
        return NULL;
    }

    while (fwrite(&res.crc, 4, 1, res.fp) < 1) {
        if (errno != EAGAIN && errno != EINTR)
            goto error;
    }

    res.fsize = 4;
    return p_dup(&res, 1);

 error:
    p_fclose(&res.fp);
    return NULL;
}

void binlog_close(binlog_t **blp)
{
    if (*blp) {
        assert (!(*blp)->dirty);
        p_fclose(&(*blp)->fp);
        p_delete(blp);
    }
}

int binlog_append(binlog_t *bl, uint16_t kind, const void *data, int len)
{
    struct binlog_hdr_t hdr = { htons(kind), htons(len) };
    struct binlog_pad_t pad = { { 0, 0, 0, 0 } , 0 };
    uint32_t crc = bl->crc;
    int pos = 0, blocks = len / 4;

    assert (bl->writing);

    memcpy(&pad.pad, (const char *)data + len - (len & 3), len & 3);
    len -= len & 3;
    crc = icrc32(crc, &hdr, sizeof(hdr));
    crc = icrc32(crc, (const char *)data, len);
    crc = icrc32(crc, &pad.pad, sizeof(pad.pad));
    pad.crc = htonl(crc);

    while (fwrite_unlocked(&hdr, sizeof(hdr), 1, bl->fp) < 1) {
        if (errno != EAGAIN && errno != EINTR)
            goto error;
    }
    do {
        size_t res = fwrite_unlocked(data + 4 * pos, 4, blocks - pos, bl->fp);
        if (res == 0 && errno != EAGAIN && errno != EINTR)
            goto error;
        pos += res;
    } while (pos < blocks);
    while (fwrite_unlocked(&pad, sizeof(pad), 1, bl->fp) < 1) {
        if (errno != EAGAIN && errno != EINTR)
            goto error;
    }

    bl->dirty  = true;
    bl->crc    = crc;
    bl->fsize += sizeof(hdr) + len + sizeof(pad);
    return 0;

  error:
    IGNORE(ftruncate(fileno(bl->fp), bl->fsize));
    fseek(bl->fp, 0, SEEK_END);
    return -1;
}

int binlog_flush(binlog_t *bl)
{
    if (bl->dirty) {
        if (fflush(bl->fp))
            return -1;
        bl->dirty = false;
    }
    return 0;
}
