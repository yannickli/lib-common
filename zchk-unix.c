/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
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
#include "z.h"

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

    Z_TEST(mkdir_p, "unix: mkdir_p") {
        t_scope;

        struct stat st;
        lstr_t absdir = t_lstr_cat(z_tmpdir_g, LSTR_IMMED_V("tst/mkdir_p"));
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

        file_in  = t_fmt(NULL, "%*pM/in",  LSTR_FMT_ARG(z_tmpdir_g));
        file_out = t_fmt(NULL, "%*pM/out", LSTR_FMT_ARG(z_tmpdir_g));

        /* Generate a string of 'size' bytes. */
        t_sb_init(&str, size + 1);
        while (str.len < size) {
            sb_addc(&str, 'a' + (str.len % 26));
        }

        /* Write it into a file. */
        Z_ASSERT_N(sb_write_file(&str, file_in));

        /* Copy this file, and check the two files have the same content. */
        Z_ASSERT_N(filecopy(file_in, file_out));
        Z_ASSERT_N(lstr_init_from_file(&out_map, file_out,
                                       PROT_READ, MAP_SHARED));
        Z_ASSERT_LSTREQUAL(out_map, LSTR_SB_V(&str));

        lstr_wipe(&out_map);
    } Z_TEST_END;
} Z_GROUP_END
