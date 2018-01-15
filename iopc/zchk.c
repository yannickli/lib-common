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

Z_GROUP_EXPORT(iopiop) {
    IOP_REGISTER_PACKAGES(&iop__pkg);

    Z_TEST(basic, "load an IOP package") {
        t_scope;
        SB_1k(err);
        SB_1k(buf);
        iop_pkg_t *pkg;
        void *foo = NULL;
        lstr_t instance_json;
        pstream_t ps;

        Z_HELPER_RUN(t_package_load(&pkg, "type.json", LSTR_NULL_V));
        instance_json = LSTR("{\"toto\":42,\"tata\":\"tutu\"}");
        ps = ps_initlstr(&instance_json);
        Z_ASSERT_N(t_iop_junpack_ptr_ps(&ps, pkg->structs[0], &foo, 0,
                                        &err), "%pL", &err);
        iop_sb_jpack(&buf, pkg->structs[0], foo,
                     IOP_JPACK_MINIMAL | IOP_JPACK_NO_TRAILING_EOL);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&buf), instance_json);

        /* TODO:
         * 1. have an IOP package similar to the one declared in the json
         *    example
         * 2. check the generated C is similar when generating from IOP or
         *    from abstract syntax.
         * 3. use the package to compare pack/unpack results.
         * 4. Have an "iop_pkg_equal" ?
         */
    } Z_TEST_END;
} Z_GROUP_END;

int main(int argc, char **argv)
{
    z_setup(argc, argv);
    z_register_exports(PLATFORM_PATH LIBCOMMON_PATH "iopc/");
    return z_run();
}
