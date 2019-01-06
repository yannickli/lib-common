/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "arith.h"
#include "unix.h"
#include "file.h"
#include "file-bin.h"
#include "z.h"

typedef struct test_struct_t {
    int32_t  a;
    uint16_t c;
    int64_t  b;
} test_struct_t;

typedef struct large_test_struct_t {
    int values[1000 / sizeof(int)];
} large_test_struct_t;

static int z_check_file_bin_records(file_bin_t *file, lstr_t path,
                                    int read_nb, int exp_nb)
{
    t_scope;
    bool do_close = false;
    qv_t(lstr) results;

    t_qv_init(&results, exp_nb);

    if (file) {
        Z_ASSERT_ZERO(file_bin_refresh(file));
    } else {
        Z_ASSERT_P((file = file_bin_open(path)));
        do_close = true;
    }

    Z_ASSERT_ZERO(t_file_bin_get_last_records(file, read_nb, &results));

    Z_ASSERT_EQ(results.len, exp_nb);

    tab_for_each_pos(pos, &results) {
        lstr_t entry = results.tab[pos];
        test_struct_t *test_ptr = entry.data;

        Z_ASSERT_EQ(entry.len, ssizeof(test_struct_t));

        Z_ASSERT_EQ(test_ptr->a, 100 - pos);
        Z_ASSERT_EQ(test_ptr->b, 100 - pos + 1);
        Z_ASSERT_EQ(test_ptr->c, 100 - pos + 2);
    }

    if (do_close) {
        Z_ASSERT_ZERO(file_bin_close(&file));
    }

    Z_HELPER_END;
}

static int z_file_bin_write_large_rec(file_bin_t *file, int recno)
{
    large_test_struct_t test;

    for (int i = 0; i < countof(test.values); i++) {
        test.values[i] = recno + i;
    }

    Z_ASSERT_N(file_bin_put_record(file, &test, sizeof(test)));

    Z_HELPER_END;
}

static int z_file_bin_check_large_rec(lstr_t record, int recno)
{
    large_test_struct_t *test;

    Z_ASSERT_EQ(record.len, ssizeof(large_test_struct_t));
    test = record.data;

    for (int i = 0; i < countof(test->values); i++) {
        Z_ASSERT_EQ(test->values[i], recno + i);
    }

    Z_HELPER_END;
}

Z_GROUP_EXPORT(file)
{
    Z_TEST(truncate, "file: truncate") {
        SB_1k(buf);
        file_t *f;
        const char *path = "zchk.file.truncate";

        Z_ASSERT_N(chdir(z_tmpdir_g.s));

        Z_ASSERT_P(f = file_open(path, FILE_WRONLY | FILE_CREATE, 0600));

        Z_ASSERT_N(file_write(f, "test12", 6));
        Z_ASSERT_N(file_flush(f));
        Z_ASSERT_N(file_truncate(f, 2));

        Z_ASSERT_N(sb_read_file(&buf, path));
        Z_ASSERT_STREQUAL("te", buf.data);

        Z_ASSERT_N(file_truncate(f, 0));

        /* test Truncate to empty */
        sb_reset(&buf);
        Z_ASSERT_N(sb_read_file(&buf, path));
        Z_ASSERT_ZERO(buf.len);
        Z_ASSERT_ZERO(f->obuf.len);
        Z_ASSERT_ZERO(f->wpos);

        /* test Truncate to 32 */
        Z_ASSERT_N(file_truncate(f, 32));
        Z_ASSERT_ZERO(f->wpos);
        Z_ASSERT_EQ(f->obuf.len, 32);

        sb_reset(&buf);
        Z_ASSERT_N(file_flush(f));
        Z_ASSERT_EQ(f->wpos, 32);
        Z_ASSERT_ZERO(f->obuf.len);

        Z_ASSERT_N(sb_read_file(&buf, path));
        Z_ASSERT_EQ(buf.len, 32);

        /* truncate test file to large buf */
        Z_ASSERT_N(file_truncate(f, 2 * BUFSIZ + 45));
        sb_reset(&buf);
        Z_ASSERT_N(sb_read_file(&buf, path));
        Z_ASSERT_EQ(buf.len, 2 * BUFSIZ + 45);

        IGNORE(file_close(&f));
        unlink(path);
    } Z_TEST_END;

    Z_TEST(file_bin, "file_bin: standard behavior") {
        t_scope;
        lstr_t file_path = t_lstr_cat(LSTR(z_tmpdir_g.s),
                                      LSTR("file_bin.test"));
        file_bin_t *file = file_bin_create(file_path, 30, true);
        /* test_struct_t size is 16 */
        test_struct_t test;
        int nbr_record = 0;

        p_clear(&test, 1);
        test.a = -1;
        test.b = UINT32_MAX + 1L;
        test.c = 1;

        Z_ASSERT_P(file);

        /* First writing */
        for (int i = 0; i < 7; i++) {
            nbr_record++;
            Z_ASSERT_N(file_bin_put_record(file, &test, sizeof(test)));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_P((file = file_bin_create(file_path, -1, false)));

        /* Second writing */
        for (int i = 0; i < 10; i++) {
            nbr_record++;
            Z_ASSERT_N(file_bin_put_record(file, &test, sizeof(test)));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_P((file = file_bin_open(file_path)));

        while (!file_bin_is_finished(file)) {
            lstr_t record;
            test_struct_t *test_ptr;

            record = file_bin_get_next_record(file);

            Z_ASSERT_EQ((unsigned)record.len, sizeof(test));
            test_ptr = record.data;

            Z_ASSERT_EQ(test_ptr->a, -1);
            Z_ASSERT_EQ(test_ptr->b, UINT32_MAX + 1L);
            Z_ASSERT_EQ(test_ptr->c, 1);

            nbr_record--;
        }
        Z_ASSERT_ZERO(nbr_record);

        Z_ASSERT_ZERO(file_bin_close(&file));
    } Z_TEST_END;

    Z_TEST(file_bin_reverse, "file_bin: reverse parsing") {
        t_scope;
        lstr_t path = t_lstr_cat(LSTR(z_tmpdir_g.s),
                                 LSTR("file_bin.test"));
        file_bin_t *file;
        file_bin_t *read_file;

#define WRITE_RECORDS(_file)  \
        do {                                                                 \
            for (int i = 0; i < 100; i++) {                                  \
                test_struct_t test = {                                       \
                    .a = i + 1,                                              \
                    .b = i + 2,                                              \
                    .c = i + 3,                                              \
                };                                                           \
                                                                             \
                Z_ASSERT_ZERO(file_bin_put_record(file, &test,               \
                                                  sizeof(test)));            \
            }                                                                \
            Z_ASSERT_ZERO(file_bin_flush(file));                             \
        } while (0)

        /* Create a file with small slots. */
        Z_ASSERT_P((file = file_bin_create(path, 30, true)));
        Z_ASSERT_P((read_file = file_bin_open(path)));

        /* Read the empty file. */
        Z_HELPER_RUN(z_check_file_bin_records(NULL,      path, 1024, 0));
        Z_HELPER_RUN(z_check_file_bin_records(read_file, path, 1024, 0));

        /* Write records. */
        WRITE_RECORDS(file);

        /* Read them. */
        Z_HELPER_RUN(z_check_file_bin_records(NULL,      path, 1024, 100));
        Z_HELPER_RUN(z_check_file_bin_records(read_file, path, 1024, 100));

        /* Read the last i records, for i from 0 to 100. */
        for (int i = 0; i <= 100; i++) {
            Z_HELPER_RUN(z_check_file_bin_records(NULL,      path, i, i));
            Z_HELPER_RUN(z_check_file_bin_records(read_file, path, i, i));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_ZERO(file_bin_close(&read_file));


        /* Same with larger slots */
        Z_ASSERT_P((file = file_bin_create(path, 500, true)));
        Z_ASSERT_P((read_file = file_bin_open(path)));

        WRITE_RECORDS(file);

        Z_HELPER_RUN(z_check_file_bin_records(NULL,      path, 1024, 100));
        Z_HELPER_RUN(z_check_file_bin_records(read_file, path, 1024, 100));

        for (int i = 0; i <= 100; i++) {
            Z_HELPER_RUN(z_check_file_bin_records(NULL,      path, i, i));
            Z_HELPER_RUN(z_check_file_bin_records(read_file, path, i, i));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_ZERO(file_bin_close(&read_file));

#undef WRITE_RECORDS
    } Z_TEST_END;

    Z_TEST(file_bin_large, "file_bin: large records") {
        t_scope;
        lstr_t file_path = t_lstr_cat(LSTR(z_tmpdir_g.s),
                                      LSTR("file_bin.test"));
        file_bin_t *file = file_bin_create(file_path, 50, true);
        int nbr_record = 0;

        Z_ASSERT_P(file);

        for (int i = 0; i < 1000; i++) {
            Z_HELPER_RUN(z_file_bin_write_large_rec(file, i));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_P((file = file_bin_open(file_path)));

        file_bin_for_each_entry(file, record) {
            Z_HELPER_RUN(z_file_bin_check_large_rec(record, nbr_record));
            nbr_record++;
        }
        Z_ASSERT_EQ(nbr_record, 1000);

        Z_ASSERT_ZERO(file_bin_close(&file));
    } Z_TEST_END;

    Z_TEST(file_bin_truncated, "file_bin: truncated file") {
        t_scope;
        lstr_t file_path = t_lstr_cat(LSTR(z_tmpdir_g.s),
                                      LSTR("file_bin.test"));
        file_bin_t *file = file_bin_create(file_path, 50, true);
        int nbr_record = 0;
        off_t pos;

        /* Write some records in file. */
        Z_HELPER_RUN(z_file_bin_write_large_rec(file, 0));
        Z_HELPER_RUN(z_file_bin_write_large_rec(file, 1));
        pos = file->cur;
        Z_HELPER_RUN(z_file_bin_write_large_rec(file, 2));

        /* Truncate the file in the middle of the last record. */
        Z_ASSERT_N(file_bin_truncate(file, pos + 50));
        Z_ASSERT_N(file_bin_close(&file));

        /* Read the records. */
        Z_ASSERT_P((file = file_bin_open(file_path)));

        file_bin_for_each_entry(file, record) {
            Z_HELPER_RUN(z_file_bin_check_large_rec(record, nbr_record));
            nbr_record++;
        }
        Z_ASSERT_EQ(nbr_record, 2);

        Z_ASSERT_N(file_bin_close(&file));
    } Z_TEST_END;

    Z_TEST(file_bin_corrupted_rec_len, "file_bin: corrupted record len") {
        t_scope;
        lstr_t file_path = t_lstr_cat(LSTR(z_tmpdir_g.s),
                                      LSTR("file_bin.test"));
        lstr_t record;
        file_bin_t *file = file_bin_create(file_path, 50, true);
        le32_t corrupted_len = cpu_to_le32(10000);
        off_t pos;

        /* Write some records in file. */
        Z_HELPER_RUN(z_file_bin_write_large_rec(file, 0));
        Z_HELPER_RUN(z_file_bin_write_large_rec(file, 1));
        pos = file->cur;
        Z_HELPER_RUN(z_file_bin_write_large_rec(file, 2));
        Z_HELPER_RUN(z_file_bin_write_large_rec(file, 3));

        /* Corrupt the len of the 3rd record */
        Z_ASSERT_N(fseek(file->f, pos, SEEK_SET));
        Z_ASSERT_EQ(fwrite(&corrupted_len, 1, sizeof(le32_t), file->f),
                    sizeof(le32_t));
        Z_ASSERT_N(file_bin_close(&file));

        /* Read the records. */
        Z_ASSERT_P((file = file_bin_open(file_path)));

        record = file_bin_get_next_record(file);
        Z_HELPER_RUN(z_file_bin_check_large_rec(record, 0));

        record = file_bin_get_next_record(file);
        Z_HELPER_RUN(z_file_bin_check_large_rec(record, 1));

        record = file_bin_get_next_record(file);
        Z_HELPER_RUN(z_file_bin_check_large_rec(record, 3));

        record = file_bin_get_next_record(file);
        Z_ASSERT_NULL(record.s);

        Z_ASSERT_N(file_bin_close(&file));
    } Z_TEST_END;

    Z_TEST(file_bin_v0, "file_bin: version 0") {
        t_scope;
        lstr_t file_path = t_lstr_cat(LSTR(z_tmpdir_g.s),
                                      LSTR("file_bin.test"));
        file_bin_t *file = file_bin_create(file_path, 1 << 20, true);
        int nbr_record = 0;

        Z_ASSERT_P(file);

        /* Let's trick the library and force it to use V0 */
        file->version = 0;

        /* +10M of datas */
        for (int i = 0; i < 10010; i++) {
            Z_HELPER_RUN(z_file_bin_write_large_rec(file, i));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_P((file = file_bin_open(file_path)));

        file_bin_for_each_entry(file, record) {
            Z_HELPER_RUN(z_file_bin_check_large_rec(record, nbr_record));
            nbr_record++;
        }
        Z_ASSERT_EQ(nbr_record, 10010);

        Z_ASSERT_ZERO(file_bin_close(&file));
    } Z_TEST_END;

    Z_TEST(mkdir_p, "unix: mkdir_p") {
        t_scope;

        struct stat st;
        lstr_t absdir = t_lstr_cat(z_tmpdir_g, LSTR("tst/mkdir_p"));
        lstr_t reldir = LSTR_IMMED("tst/mkdir_p2");

        Z_ASSERT_N(chdir(z_tmpdir_g.s));

        Z_ASSERT_EQ(1, mkdir_p(absdir.s, 0755));
        Z_ASSERT_N(stat(absdir.s, &st));
        Z_ASSERT(S_ISDIR(st.st_mode));
        Z_ASSERT_EQ(0, mkdir_p(absdir.s, 0755));
        Z_ASSERT_N(stat(absdir.s, &st));
        Z_ASSERT(S_ISDIR(st.st_mode));

        Z_ASSERT_EQ(1, mkdir_p(reldir.s, 0755));
        Z_ASSERT_N(stat(reldir.s, &st));
        Z_ASSERT(S_ISDIR(st.st_mode));
        Z_ASSERT_EQ(0, mkdir_p(reldir.s, 0755));
        Z_ASSERT_N(stat(reldir.s, &st));
        Z_ASSERT(S_ISDIR(st.st_mode));
    } Z_TEST_END;

    Z_TEST(filecopy, "filecopy") {
        t_scope;
        const int size = 10 << 20; /* 10 MB */
        const char *file_in;
        const char *file_out;
        lstr_t out_map;
        sb_t str;

        file_in  = t_fmt("%*pM/in",  LSTR_FMT_ARG(z_tmpdir_g));
        file_out = t_fmt("%*pM/out", LSTR_FMT_ARG(z_tmpdir_g));

        /* Generate a string of 'size' bytes. */
        t_sb_init(&str, size + 1);
        while (str.len < size) {
            sb_addc(&str, 'a' + (str.len % 26));
        }

        /* Write it into a file. */
        Z_ASSERT_N(sb_write_file(&str, file_in), "%m");

        /* Copy this file, and check the two files have the same content. */
        Z_ASSERT_N(filecopy(file_in, file_out));
        Z_ASSERT_N(lstr_init_from_file(&out_map, file_out,
                                       PROT_READ, MAP_SHARED));
        Z_ASSERT_LSTREQUAL(out_map, LSTR_SB_V(&str));

        lstr_wipe(&out_map);
    } Z_TEST_END;

    Z_TEST(xappend_to_file, "xappend_to_file") {
        t_scope;
        const char *appending_file;
        const char *str1 = "toto";
        const char *str2 = "titi";
        SB_1k(res);
        SB_1k(fcontent);

        appending_file = t_fmt("%*pM/appending",  LSTR_FMT_ARG(z_tmpdir_g));

        Z_ASSERT_N(xappend_to_file(appending_file, str1, strlen(str1)));
        Z_ASSERT_N(xappend_to_file(appending_file, str2, strlen(str2)));

        sb_addf(&res, "%s%s", str1, str2);
        Z_ASSERT_N(sb_read_file(&fcontent, appending_file));

        Z_ASSERT_STREQUAL(fcontent.data, res.data);
    } Z_TEST_END;
} Z_GROUP_END
