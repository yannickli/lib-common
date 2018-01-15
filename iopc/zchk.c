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

static int test_package(const char *pkg_file, int st_index,
                        const char *_st_json)
{
    t_scope;
    SB_1k(err);
    SB_1k(buf);
    iop_pkg_t *pkg;
    void *st_ptr = NULL;
    pstream_t ps;
    lstr_t st_json = LSTR(_st_json);
    const iop_struct_t *st_desc;

    Z_HELPER_RUN(t_package_load(&pkg, pkg_file, LSTR_NULL_V));
    ps = ps_initlstr(&st_json);
    st_desc = pkg->structs[st_index];
    Z_ASSERT_N(t_iop_junpack_ptr_ps(&ps, st_desc, &st_ptr, st_index, &err),
               "%pL", &err);
    iop_sb_jpack(&buf, st_desc, st_ptr,
                 IOP_JPACK_MINIMAL | IOP_JPACK_NO_TRAILING_EOL);
    Z_ASSERT_LSTREQUAL(LSTR_SB_V(&buf), st_json);
    Z_HELPER_END;
}

Z_GROUP_EXPORT(iopiop) {
    IOP_REGISTER_PACKAGES(&iop__pkg);

    Z_TEST(basic, "load an IOP package") {
        Z_HELPER_RUN(test_package("type.json", 0,
                                  "{\"toto\":42,\"tata\":\"tutu\"}"));
    } Z_TEST_END;

    Z_TEST(sub_struct, "struct with struct field") {
        Z_HELPER_RUN(test_package("sub-struct.json", 1,
                                  "{\"st\":{\"i\":51}}"));
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
