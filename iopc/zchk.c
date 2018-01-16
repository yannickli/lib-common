/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "iopc-iop.h"
#include "../iop/tstiop.iop.h"

#include <lib-common/z.h>

static const char *t_get_path(const char *filename)
{
    return t_fmt("%pL/iopioptests/%s", &z_cmddir_g, filename);
}

static int t_package_load(iop_pkg_t **pkg, const char *file,
                          lstr_t err_msg)
{
    SB_1k(err);
    const char *path;
    iop__package__t pkg_desc;

    path = t_get_path(file);
    Z_ASSERT_N(t_iop_junpack_file(path, &iop__package__s, &pkg_desc, 0,
                                  NULL, &err), "%s: %pL", file, &err);
    *pkg = mp_iop_pkg_from_desc(t_pool(), &pkg_desc, &err);
    if (err_msg.s) {
        Z_ASSERT_NULL(*pkg, "%s: expected an error", file);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&err), err_msg,
                           "%s: unexpected error message", file);
    } else {
        Z_ASSERT_P(*pkg, "%s: %pL", file, &err);
    }

    Z_HELPER_END;
}

static int _t_test_struct(const char *pkg_file, int st_index,
                          const char **jsons, int nb_jsons,
                          const iop_struct_t **_st_desc)
{
    SB_1k(err);
    SB_1k(buf);
    iop_pkg_t *pkg;
    const iop_struct_t *st_desc;

    Z_HELPER_RUN(t_package_load(&pkg, pkg_file, LSTR_NULL_V));
    st_desc = pkg->structs[st_index];

    for (int i = 0; i < nb_jsons; i++) {
        lstr_t st_json = LSTR(jsons[i]);
        pstream_t ps;
        void *st_ptr = NULL;

        ps = ps_initlstr(&st_json);
        Z_ASSERT_N(t_iop_junpack_ptr_ps(&ps, st_desc, &st_ptr, st_index, &err),
                   "cannot junpack `%pL': %pL", &err, &st_json);
        sb_reset(&buf);
        Z_ASSERT_N(iop_sb_jpack(&buf, st_desc, st_ptr,
                                IOP_JPACK_MINIMAL |
                                IOP_JPACK_NO_TRAILING_EOL),
                   "cannot repack to get `%pL'", &st_json);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&buf), st_json,
                           "the json data changed after unpack/repack");
    }

    if (_st_desc) {
        *_st_desc = st_desc;
    }

    Z_HELPER_END;
}

#define t_test_struct(_file, _idx, st_desc, ...)                             \
    ({                                                                       \
        const char *__files[] = { __VA_ARGS__ };                             \
                                                                             \
        _t_test_struct(_file, _idx, __files, countof(__files), (st_desc));   \
    })

#define test_struct(_file, _idx, ...)                                        \
    ({                                                                       \
        t_scope;                                                             \
        t_test_struct(_file, _idx, NULL, ##__VA_ARGS__);                     \
    })

Z_GROUP_EXPORT(iopiop) {
    IOP_REGISTER_PACKAGES(&iop__pkg);

    Z_TEST(struct_, "basic struct") {
        Z_HELPER_RUN(test_struct("struct.json", 0,
                                 "{\"i1\":42,\"i2\":2,\"s\":\"foo\"}"));
    } Z_TEST_END;

    Z_TEST(sub_struct, "struct with struct field") {
        t_scope;
        const char *v1 = "{\"i\":51}";
        const char *v2 = "{\"i\":12345678}";
        const char *tst1;
        const char *tst2;
        const iop_struct_t *st;
        const iop_struct_t *ref_st;

        tst1 = t_fmt("{\"st\":%s,\"stRef\":%s}", v1, v2);
        tst2 = t_fmt("{\"st\":%s,\"stRef\":%s,\"stOpt\":%s}", v1, v2, v1);

        Z_HELPER_RUN(t_test_struct("sub-struct.json", 1, &st, tst1, tst2));

        ref_st = tstiop__s2__sp;
        Z_ASSERT_EQ(st->fields_len, ref_st->fields_len);
        for (int i = 0; i < st->fields_len; i++) {
            const iop_field_t *fdesc = &st->fields[i];
            const iop_field_t *ref_fdesc = &ref_st->fields[i];

            Z_ASSERT_LSTREQUAL(fdesc->name, ref_fdesc->name);
            Z_ASSERT_EQ(fdesc->size, ref_fdesc->size,
                        "sizes differ for field `%pL'", &fdesc->name);
        }
    } Z_TEST_END;

    Z_TEST(union_, "basic union") {
        Z_HELPER_RUN(test_struct("union.json", 0,
                                 "{\"i\":6}",
                                 "{\"s\":\"toto\"}"));
    } Z_TEST_END;

    Z_TEST(error, "try to load a broken IOP package") {
        t_scope;
        iop_pkg_t *pkg;
        const char *err;

        err = "failed to resolve the package: "
              "error: unable to find any pkg providing type `Unknown`";

        Z_HELPER_RUN(t_package_load(&pkg, "error.json", LSTR(err)));
    } Z_TEST_END;
} Z_GROUP_END;

int main(int argc, char **argv)
{
    z_setup(argc, argv);
    z_register_exports(PLATFORM_PATH LIBCOMMON_PATH "iopc/");
    return z_run();
}
