/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2014 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "log.h"
#include "file-bin.h"

/* File header */
#define CURRENT_VERSION  1

/* Should always be 16 characters long */
#define SIG_0100  "IS_binary/v01.0\0"
#define SIG       SIG_0100

typedef struct file_bin_header_t {
    char   version[16];
    le32_t slot_size;
} file_bin_header_t;

#define HEADER_VERSION_SIZE  16
#define HEADER_SIZE(file) \
    (file->version == 0 ? 0 : ssizeof(file_bin_header_t))

/* Slot header */
typedef le32_t slot_hdr_t;

#define SLOT_HDR_SIZE(file)  \
    ((file)->version == 0 ? 0 : ssizeof(slot_hdr_t))

/* Record header */
typedef le32_t rc_hdr_t;
#define RC_HDR_SIZE  ssizeof(rc_hdr_t)

static struct {
    logger_t logger;
} file_bin_g = {
#define _G  file_bin_g
    .logger = LOGGER_INIT_INHERITS(NULL, "file_bin"),
};

/* {{{ Helpers */

static uint32_t file_bin_remaining_space_in_slot(const file_bin_t *file)
{
    return file->slot_size - (file->cur % file->slot_size);
}

static off_t file_bin_get_prev_slot(const file_bin_t *file, off_t pos)
{
    return pos < file->slot_size ? HEADER_SIZE(file)
                                 : pos - (pos % file->slot_size);
}

static bool is_at_slot_start(const file_bin_t *f)
{
    return f->cur % f->slot_size == 0 || f->cur == HEADER_SIZE(f);
}

static int file_bin_seek(file_bin_t *file, off_t offset, int whence)
{
    if (fseek(file->f, offset, whence) < 0) {
        return logger_error(&_G.logger, "cannot use fseek on file "
                            "'%*pM': %m", LSTR_FMT_ARG(file->path));
    }

    return 0;
}

static off_t file_bin_tell(const file_bin_t *file)
{
    off_t res = ftell(file->f);

    if (res < 0) {
        logger_error(&_G.logger, "cannot use ftell on file '%*pM': %m",
                     LSTR_FMT_ARG(file->path));
    }

    return res;
}

static off_t file_bin_get_next_entry_off(file_bin_t *f, uint32_t d_len)
{
    /* Number of slot header in this range */
    int tmp_header = 1;
    off_t res;
    off_t prev_end = f->cur + d_len;
    int n_slot_header = get_multiples_nb_in_range(f->slot_size, f->cur,
                                                  f->cur + d_len);

    /* Even if not a multiple of slot_size, offset HEADER_SIZE needs a slot
     * header
     */
    if (f->cur == HEADER_SIZE(f) && f->version > 0) {
        n_slot_header++;
    }

    res = f->cur + d_len + SLOT_HDR_SIZE(f) * n_slot_header;

    /* Adding slot headers could have move us behind other slot beginning */
    while (tmp_header > 0 && res > prev_end) {
        tmp_header = get_multiples_nb_in_range(f->slot_size,
                                               prev_end + 1, res);
        prev_end = res;
        res += SLOT_HDR_SIZE(f) * tmp_header;
    }

    /* If not enough space to put a record header, let's jump to the next slot
     * beginning
     */
    if (f->slot_size - (res % f->slot_size) < RC_HDR_SIZE) {
        res += f->slot_size - (res % f->slot_size) + SLOT_HDR_SIZE(f);
    }

    return res;
}

/* }}} */
/* {{{ Reading */

int file_bin_refresh(file_bin_t *file)
{
    struct stat st;
    void *new_map;

    if (fstat(fileno(file->f), &st) < 0) {
        return logger_error(&_G.logger, "cannot stat file '%*pM': %m",
                            LSTR_FMT_ARG(file->path));
    }

    if (file->length == st.st_size) {
        return 0;
    }

    new_map = mremap(file->map, file->length, st.st_size, MREMAP_MAYMOVE);

    if (new_map == MAP_FAILED) {
        return logger_error(&_G.logger, "cannot remap file '%*pM': %m",
                            LSTR_FMT_ARG(file->path));
    }

    file->map = new_map;
    file->length = st.st_size;

    return 0;
}

int _file_bin_seek(file_bin_t *file, off_t pos)
{
    /* If this one fails, you are probably looking for file_bin_truncate. */
    assert (file->map);

    THROW_ERR_UNLESS(expect(pos <= file->length));

    file->cur = pos;

    return 0;
}

static int file_bin_parse_header(lstr_t path, void *data, uint16_t *version,
                                 uint32_t *slot_size)
{
    file_bin_header_t *header = data;

    if (lstr_equal2(LSTR(header->version), LSTR(SIG_0100))) {
        *version = 1;
        *slot_size = le_to_cpu32p(&header->slot_size);
        goto end;
    }

    /* Unknown header. File is probably in version 0 */
    *version = 0;
    *slot_size = FILE_BIN_DEFAULT_SLOT_SIZE;

  end:
    logger_trace(&_G.logger, 3, "parsed file header for '%*pM': version = %u,"
                 " slot size = %u", LSTR_FMT_ARG(path), *version, *slot_size);
    return 0;
}

static int file_bin_get_cpu32(file_bin_t *file, uint32_t *res)
{
    pstream_t ps = ps_init(file->map + file->cur, sizeof(le32_t));
    le32_t le32;

    if (ps_get_le32(&ps, &le32) < 0) {
        return logger_error(&_G.logger, "cannot read le32 at offset '%ju' "
                            "for file '%*pM'", file->cur,
                            LSTR_FMT_ARG(file->path));
    }

    *res = le32;
    file->cur += sizeof(le32_t);

    return 0;
}

static lstr_t
_file_bin_get_next_record(file_bin_t *file)
{
    uint32_t sz;
    uint32_t check_slot_hdr;
    bool is_spanning = false;

    if (!expect(!file_bin_is_finished(file))) {
        return LSTR_NULL_V;
    }

    if (file->version > 0) {
        file->cur = MAX(file->cur, HEADER_SIZE(file));
    }

    sb_reset(&file->record_buf);

    if (file_bin_remaining_space_in_slot(file) < RC_HDR_SIZE) {
        file->cur += file_bin_remaining_space_in_slot(file);
    }

    while (is_at_slot_start(file) && file->version > 0) {
        uint32_t next_entry;

        if (file_bin_get_cpu32(file, &next_entry) < 0) {
            goto jump;
        }
        if (next_entry > file->length - RC_HDR_SIZE) {
            logger_error(&_G.logger, "buggy slot header in file '%*pM' at "
                         "offset %jd, next entry is supposed to be at offset "
                         "%jd whereas file is %u bytes long",
                         LSTR_FMT_ARG(file->path),
                         file->cur - SLOT_HDR_SIZE(file),
                         file->cur + next_entry, file->length);
            goto jump;
        }

        file->cur += next_entry;
    }

    if (file_bin_get_cpu32(file, &sz) < 0) {
        goto jump;
    }

    if (file->cur + sz > file->length) {
        logger_error(&_G.logger, "buggy record header in file '%*pM' at "
                     "offset %jd, size is %u and does "
                     "not match the remaining data in the file (%ld)",
                     LSTR_FMT_ARG(file->path),
                     file->cur - SLOT_HDR_SIZE(file), sz,
                     file->length - file->cur);
        goto jump;
    }

    if (is_at_slot_start(file)) {
        file->cur += SLOT_HDR_SIZE(file);
    }

    if (sz == 0) {
        goto jump;
    }

    check_slot_hdr = file_bin_get_next_entry_off(file, sz);

    while (!file_bin_is_finished(file)) {
        uint32_t remaining = file_bin_remaining_space_in_slot(file);
        slot_hdr_t tmp_size;

        remaining = MIN(remaining, file->length - file->cur);

        if (sz <= remaining) {
            /* The last part of this record is in this slot. */
            lstr_t res = LSTR_DATA(file->map + file->cur, sz);

            file->cur += sz;

            if (!is_spanning) {
                return res;
            } else {
                sb_add_lstr(&file->record_buf, res);

                return LSTR_SB_V(&file->record_buf);
            }
        }

        assert (file->version > 0);

        /* Record span on multiple slots. */
        if (!is_spanning) {
            is_spanning = true;
            sb_grow(&file->record_buf, sz);
        }

        sb_add(&file->record_buf, file->map + file->cur, remaining);
        sz -= remaining;
        file->cur += remaining;

        if (!expect(is_at_slot_start(file))) {
            logger_error(&_G.logger, "corrupted binary file, a slot start was"
                         " expected at pos %jd", file->cur);
            goto jump;
        }

        /* Consuming slot header */
        if (file_bin_get_cpu32(file, &tmp_size) < 0) {
            goto jump;
        }

        if (tmp_size != check_slot_hdr - file->cur) {
            logger_error(&_G.logger, "buggy slot header in file '%*pM', "
                         "expected %ld, got %u, jumping to next slot",
                         LSTR_FMT_ARG(file->path), check_slot_hdr - file->cur,
                         tmp_size);
            goto jump;
        }
    }

    assert (false);
  jump:
    file->cur += file_bin_remaining_space_in_slot(file);
    return LSTR_NULL_V;
}

lstr_t file_bin_get_next_record(file_bin_t *file)
{
    lstr_t res = LSTR_NULL_V;

    while (!res.s && !file_bin_is_finished(file)) {
        res = _file_bin_get_next_record(file);
    }

    return res;
}

int t_file_bin_get_last_records(file_bin_t *file, int count, qv_t(lstr) *out)
{
    off_t save_cur = file->cur;
    off_t slot_off = file->cur = file_bin_get_prev_slot(file, file->length);
    qv_t(lstr) tmp;

    t_qv_init(lstr, &tmp, count);

    while (slot_off >= 0 && count > 0) {
        while (file->cur < (slot_off + file->slot_size - SLOT_HDR_SIZE(file))
            && count > 0 && !file_bin_is_finished(file))
        {
            lstr_t res = file_bin_get_next_record(file);

            if (res.s) {
                qv_append(lstr, &tmp, t_lstr_dup(res));
                count--;
            }
        }

        qv_grow(lstr, out, tmp.len);
        for (int pos = tmp.len; pos-- > 0;) {
            qv_append(lstr, out, tmp.tab[pos]);
        }

        qv_clear(lstr, &tmp);
        slot_off = file_bin_get_prev_slot(file, slot_off - 1);
        file->cur = slot_off;
    }

    file->cur = save_cur;

    return 0;
}

file_bin_t *file_bin_open(lstr_t path)
{
    file_bin_t *res;
    struct stat st;
    void *mapping = NULL;
    uint16_t version = 0;
    uint32_t slot_size = 1 << 20;
    lstr_t r_path = lstr_dup(path);
    FILE *file = fopen(r_path.s, "r");

    if (!file) {
        logger_error(&_G.logger, "cannot open file '%*pM: %m",
                     LSTR_FMT_ARG(path));
        goto error;
    }

    if (fstat(fileno(file), &st) < 0) {
        logger_error(&_G.logger, "cannot get stat on file '%*pM': %m",
                     LSTR_FMT_ARG(path));
        goto error;
    }

    if (st.st_size < 0) {
        logger_error(&_G.logger, "empty binary file '%*pM'",
                     LSTR_FMT_ARG(path));
        goto error;
    }

    if (st.st_size > 0) {
        mapping = mmap(NULL, st.st_size, PROT_READ,
                       MAP_SHARED, fileno(file), 0);
        if (mapping == MAP_FAILED) {
            logger_error(&_G.logger, "cannot map file '%*pM': %m",
                         LSTR_FMT_ARG(path));
            goto error;
        }

        if (file_bin_parse_header(path, mapping, &version, &slot_size) < 0) {
            goto error;
        }
    }

    res = file_bin_new();
    res->f = file;
    res->path = r_path;
    res->length = st.st_size;
    res->map = mapping;
    res->version = version;
    res->slot_size = slot_size;
    res->cur = HEADER_SIZE(res);
    sb_init(&res->record_buf);

    return res;

  error:
    p_fclose(&file);
    lstr_wipe(&r_path);
    return NULL;
}

/* }}} */
/* {{{ Writing */

int file_bin_flush(file_bin_t *file)
{
    if (fflush(file->f) < 0) {
        return logger_error(&_G.logger, "cannot flush file '%*pM': %m",
                            LSTR_FMT_ARG(file->path));
    }

    return 0;
}

int file_bin_sync(file_bin_t *file)
{
    RETHROW(file_bin_flush(file));

    if (fsync(fileno(file->f)) < 0) {
        return logger_error(&_G.logger, "cannot sync file '%*pM': %m",
                            LSTR_FMT_ARG(file->path));
    }

    return 0;
}

int file_bin_truncate(file_bin_t *file, off_t pos)
{
    RETHROW(file_bin_flush(file));

    if (xftruncate(fileno(file->f), pos) < 0) {
        return logger_error(&_G.logger, "cannot truncate file '%*pM' at pos "
                            "%jd: %m", LSTR_FMT_ARG(file->path), pos);
    }

    file->cur = MIN(file->cur, pos);

    RETHROW(file_bin_seek(file, file->cur, SEEK_SET));

    return 0;
}

static int file_bin_pad(file_bin_t *file, off_t new_pos)
{
    off_t real_cur = ftell(file->f);

    if (real_cur < 0) {
        return logger_error(&_G.logger, "cannot use ftell on file '%*pM': %m",
                            LSTR_FMT_ARG(file->path));
    }

    THROW_ERR_UNLESS(expect(real_cur <= new_pos));

    if (real_cur == new_pos) {
        return 0;
    }

    RETHROW(file_bin_truncate(file, new_pos));

    real_cur = RETHROW(file_bin_tell(file));

    THROW_ERR_UNLESS(expect(real_cur == new_pos));

    return 0;
}

/* Write opaque data in file starting at offset file->cur */
static int _write_file_bin(file_bin_t *file, const void *data, uint32_t len)
{
    uint32_t res;

    RETHROW(file_bin_pad(file, file->cur));

    res = fwrite(data, 1, len, file->f);

    if (res < len) {
        IGNORE(file_bin_truncate(file, file->cur));
        return logger_error(&_G.logger, "cannot write in file '%*pM': %m",
                            LSTR_FMT_ARG(file->path));
    }

    file->cur += res;

    return 0;
}

static int file_bin_write_header(file_bin_t *file)
{
    lstr_t version_str = LSTR_DATA(SIG, 16);
    le32_t slot_size = cpu_to_le32(file->slot_size);

    RETHROW(_write_file_bin(file, version_str.data, version_str.len));
    RETHROW(_write_file_bin(file, &slot_size, sizeof(le32_t)));

    return 0;
}

static int file_bin_write_slot_header(file_bin_t *file, off_t next_entry)
{
    off_t to_write = next_entry - (file->cur + SLOT_HDR_SIZE(file));
    slot_hdr_t towrite_le32 = cpu_to_le32((uint32_t)to_write);

    assert (next_entry >= (long)(file->cur + SLOT_HDR_SIZE(file)));
    assert (to_write <= UINT32_MAX);

    return _write_file_bin(file, &towrite_le32, SLOT_HDR_SIZE(file));
}

/* Write data in file and write slot headers when necessary */
static int file_bin_write_data(file_bin_t *f, const void *data, uint32_t len,
                               off_t next_entry, bool r_start)
{
    while (len > 0) {
        uint32_t w_size;

        if (is_at_slot_start(f)) {
            if (r_start) {
                file_bin_write_slot_header(f, f->cur + SLOT_HDR_SIZE(f));
            } else {
                file_bin_write_slot_header(f, next_entry);
            }
        }

        w_size = MIN(file_bin_remaining_space_in_slot(f), len);

        len -= w_size;
        RETHROW(_write_file_bin(f, data, w_size));

        data = (byte *)data + w_size;
        r_start = false;
    }

    return 0;
}

int file_bin_put_record(file_bin_t *file, const void *data, uint32_t len)
{
    rc_hdr_t rec_len_le32 = cpu_to_le32(len);
    uint32_t total_size = len + RC_HDR_SIZE;
    off_t next_entry;
    uint32_t remaining;

    if (file->cur == 0 && file->version > 0) {
        /* File beginning */
        RETHROW(file_bin_write_header(file));
    }

    remaining = file_bin_remaining_space_in_slot(file);

    if (remaining < RC_HDR_SIZE
    ||  (file->version == 0 && remaining < total_size))
    {
        file->cur += remaining;
    }
    next_entry = file_bin_get_next_entry_off(file, total_size);

    RETHROW(file_bin_write_data(file, &rec_len_le32, RC_HDR_SIZE,
                                next_entry, true));
    RETHROW(file_bin_write_data(file, data, len, next_entry, false));

    return 0;
}

file_bin_t *file_bin_create(lstr_t path, uint32_t slot_size, bool trunc)
{
    file_bin_t *res;
    FILE *file;
    lstr_t r_path = lstr_dup(path);

    file = fopen(r_path.s, trunc ? "w" : "a+");
    if (!file) {
        logger_error(&_G.logger, "cannot open file '%*pM': %m",
                     LSTR_FMT_ARG(path));
        lstr_wipe(&r_path);
        return NULL;
    }

#define GOTO_ERROR_IF_FAIL(expr)  if (unlikely((expr) < 0)) { goto error; }

    slot_size = slot_size > 0 ? slot_size : FILE_BIN_DEFAULT_SLOT_SIZE;

    res = file_bin_new();
    res->f = file;
    res->path = r_path;

    GOTO_ERROR_IF_FAIL(file_bin_seek(res, 0, SEEK_END));
    GOTO_ERROR_IF_FAIL((res->cur = file_bin_tell(res)));

    res->version = CURRENT_VERSION;
    res->slot_size = slot_size;

    if (res->cur > 0) {
        /* Appending an already existing file */
        byte buf[20];
        uint16_t ver;
        uint32_t tmp_slot_sz;

        if (res->cur < 20) {
            res->slot_size = FILE_BIN_DEFAULT_SLOT_SIZE;
            res->version = 0;
            return res;
        }

        rewind(file);
        if (fread(buf, 1, 20, file) < 20) {
            logger_error(&_G.logger, "cannot read binary file header "
                         "for file '%*pM': %m", LSTR_FMT_ARG(r_path));
            goto error;
        }

        GOTO_ERROR_IF_FAIL(file_bin_seek(res, 0, SEEK_END));
        GOTO_ERROR_IF_FAIL(file_bin_parse_header(r_path, buf, &ver,
                                                 &tmp_slot_sz));

        res->slot_size = tmp_slot_sz;
        res->version = ver;
        return res;
    }

    if (unlikely(slot_size < HEADER_SIZE(res) + SLOT_HDR_SIZE(res))) {
        logger_error(&_G.logger, "slot size should be higher than %ld, got "
                     " %u for file '%*pM'",
                     HEADER_SIZE(res) + SLOT_HDR_SIZE(res), slot_size,
                     LSTR_FMT_ARG(r_path));
        goto error;
    }

    return res;

#undef GOTO_ERROR_IF_FAIL

  error:
    IGNORE(file_bin_close(&res));
    return NULL;
}

/* }}} */

int file_bin_close(file_bin_t **file_ptr)
{
    int res = 0;
    file_bin_t *file = *file_ptr;

    if (!file) {
        return 0;
    }

    if (file->map) {
        if (munmap(file->map, file->length) < 0) {
            res = logger_error(&_G.logger, "cannot unmap file '%*pM': %m",
                               LSTR_FMT_ARG(file->path));
        }
        sb_wipe(&file->record_buf);
    }

    if (p_fclose(&file->f) < 0) {
        res = logger_error(&_G.logger, "cannot close file '%*pM': %m",
                           LSTR_FMT_ARG(file->path));
    }

    file_bin_delete(file_ptr);

    return res;
}
