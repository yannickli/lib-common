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
    char buf[1000];
} large_test_struct_t;

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
        test_struct_t test = {.a = -1, .b = UINT32_MAX + 1, .c = 1};
        int nbr_record = 0;

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
            lstr_t record = file_bin_get_next_record(file);
            test_struct_t *test_ptr;

            Z_ASSERT_EQ((unsigned)record.len, sizeof(test));
            test_ptr = record.data;

            Z_ASSERT_EQ(test_ptr->a, -1);
            Z_ASSERT_EQ(test_ptr->b, UINT32_MAX + 1);
            Z_ASSERT_EQ(test_ptr->c, 1);

            nbr_record--;
        }
        Z_ASSERT_ZERO(nbr_record);

        Z_ASSERT_ZERO(file_bin_close(&file));
    } Z_TEST_END;

    Z_TEST(file_bin_reverse, "file_bin: reverse parsing") {
        t_scope;
        lstr_t file_path = t_lstr_cat(LSTR(z_tmpdir_g.s),
                                      LSTR("file_bin.test"));
        file_bin_t *file = file_bin_create(file_path, 25, true);
        test_struct_t test = {.a = 1, .b = 2, .c = 3};
        qv_t(lstr) results;

        Z_ASSERT_P(file);

        t_qv_init(lstr, &results, 100);

        for (int i = 0; i < 100; i++) {
            Z_ASSERT_ZERO(file_bin_put_record(file, &test, sizeof(test)));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_P((file = file_bin_open(file_path)));

        Z_ASSERT_ZERO(t_file_bin_get_last_records(file, 100, &results));
        Z_ASSERT_EQ(results.len, 100);

        qv_for_each_entry(lstr, entry, &results) {
            test_struct_t *test_ptr = entry.data;

            Z_ASSERT_EQ(test_ptr->a, 1);
            Z_ASSERT_EQ(test_ptr->b, 2);
            Z_ASSERT_EQ(test_ptr->c, 3);
        }

        Z_ASSERT_ZERO(file_bin_close(&file));

        /* Same with larger slots */
        qv_clear(lstr, &results);
        Z_ASSERT_P((file = file_bin_create(file_path, 500, true)));

        for (int i = 0; i < 100; i++) {
            Z_ASSERT_ZERO(file_bin_put_record(file, &test, sizeof(test)));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_P((file = file_bin_open(file_path)));

        Z_ASSERT_ZERO(t_file_bin_get_last_records(file, 100, &results));
        Z_ASSERT_EQ(results.len, 100);

        qv_for_each_entry(lstr, entry, &results) {
            test_struct_t *test_ptr = entry.data;

            Z_ASSERT_EQ(test_ptr->a, 1);
            Z_ASSERT_EQ(test_ptr->b, 2);
            Z_ASSERT_EQ(test_ptr->c, 3);
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
    } Z_TEST_END;

    Z_TEST(file_bin_large, "file_bin: large records") {
        t_scope;
        lstr_t file_path = t_lstr_cat(LSTR(z_tmpdir_g.s),
                                      LSTR("file_bin.test"));
        file_bin_t *file = file_bin_create(file_path, 50, true);
        /* large_test_struct_t size is 1000 */
        large_test_struct_t test;
        int nbr_record = 0;

        Z_ASSERT_P(file);

        for (int i = 0; i < 100; i++) {
            for (int j = 0; j < 10; j++) {
                test.buf[i * 10 + j] = j;
            }
        }

        for (int i = 0; i < 10000; i++) {
            nbr_record++;
            Z_ASSERT_N(file_bin_put_record(file, &test, sizeof(test)));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_P((file = file_bin_open(file_path)));

        file_bin_for_each_entry(file, record) {
            large_test_struct_t *test_ptr;

            Z_ASSERT_EQ((unsigned)record.len, sizeof(test));
            test_ptr = record.data;

            for (int i = 0; i < 100; i++) {
                for (int j = 0; j < 10; j++) {
                    Z_ASSERT_EQ(test_ptr->buf[i * 10 + j], j);
                }
            }

            nbr_record--;
        }
        Z_ASSERT_ZERO(nbr_record);

        Z_ASSERT_ZERO(file_bin_close(&file));
    } Z_TEST_END;

    Z_TEST(file_bin_v0, "file_bin: version 0") {
        t_scope;
        lstr_t file_path = t_lstr_cat(LSTR(z_tmpdir_g.s),
                                      LSTR("file_bin.test"));
        file_bin_t *file = file_bin_create(file_path, 1 << 20, true);
        large_test_struct_t test;
        int nbr_record = 0;

        Z_ASSERT_P(file);

        /* Let's trick the library and force it to use V0 */
        file->version = 0;

        for (int i = 0; i < 100; i++) {
            for (int j = 0; j < 10; j++) {
                test.buf[i * 10 + j] = j;
            }
        }

        /* +10M of datas */
        for (int i = 0; i < 10010; i++) {
            nbr_record++;
            Z_ASSERT_N(file_bin_put_record(file, &test, sizeof(test)));
        }

        Z_ASSERT_ZERO(file_bin_close(&file));
        Z_ASSERT_P((file = file_bin_open(file_path)));

        file_bin_for_each_entry(file, record) {
            large_test_struct_t *test_ptr;

            Z_ASSERT_EQ((unsigned)record.len, sizeof(test));
            test_ptr = record.data;

            for (int i = 0; i < 100; i++) {
                for (int j = 0; j < 10; j++) {
                    Z_ASSERT_EQ(test_ptr->buf[i * 10 + j], j);
                }
            }

            nbr_record--;
        }
        Z_ASSERT_ZERO(nbr_record);

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
} Z_GROUP_END
