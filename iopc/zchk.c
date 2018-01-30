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

#include "iopc-iopsq.h"
#include "../iop/tstiop.iop.h"

#include <lib-common/z.h>

/* {{{ Helpers */

static const char *t_get_path(const char *filename)
{
    return t_fmt("%pL/iopsq-tests/%s", &z_cmddir_g, filename);
}

static lstr_t t_build_json_pkg(const char *pkg_name)
{
    return t_lstr_fmt("{\"name\":\"%s\",\"elems\":[]}", pkg_name);
}

static iop__package__t *t_load_package_from_file(const char *filename,
                                                 sb_t *err)
{
    const char *path;
    iop__package__t *pkg_desc = NULL;

    path = t_get_path(filename);
    RETHROW_NP(t_iop_junpack_ptr_file(path, &iop__package__s,
                                      (void **)&pkg_desc, 0, NULL, err));

    return pkg_desc;
}

/* }}} */
/* {{{ Z_HELPERs */

static int t_package_load(iop_pkg_t **pkg, const char *file,
                          lstr_t err_msg)
{
    SB_1k(err);
    const iop__package__t *pkg_desc;

    pkg_desc = t_load_package_from_file(file, &err);
    Z_ASSERT_P(pkg_desc, "%s: %pL", file, &err);
    *pkg = mp_iopsq_build_pkg(t_pool(), pkg_desc, &err);
    if (err_msg.s) {
        Z_ASSERT_NULL(*pkg, "%s: expected an error", file);
        Z_ASSERT_LSTREQUAL(LSTR_SB_V(&err), err_msg,
                           "%s: unexpected error message", file);
    } else {
        Z_ASSERT_P(*pkg, "%s: %pL", file, &err);
    }

    Z_HELPER_END;
}

static int z_assert_ranges_eq(const int *ranges, int ranges_len,
                              const int *ref_ranges, int ref_ranges_len)
{
    Z_ASSERT_EQ(ranges_len, ref_ranges_len, "lengths mismatch");
    for (int i = 0; i < ranges_len * 2 + 1; i++) {
        Z_ASSERT_EQ(ranges[i], ref_ranges[i],
                    "ranges differ at index %d", i);
    }

    Z_HELPER_END;
}

static int z_assert_enum_eq(const iop_enum_t *en, const iop_enum_t *ref)
{
    if (en == ref) {
        return 0;
    }

    Z_ASSERT_LSTREQUAL(en->name, ref->name, "names mismatch");
    /* XXX Don't check fullname: the package name can change. */

    Z_ASSERT_EQ(en->enum_len, ref->enum_len, "length mismatch");
    for (int i = 0; i < en->enum_len; i++) {
        Z_ASSERT_LSTREQUAL(en->names[i], ref->names[i],
                           "names mismatch for element #%d", i);
        Z_ASSERT_EQ(en->values[i], ref->values[i],
                    "values mismatch for element #%d", i);
    }

    Z_ASSERT_EQ(en->flags, ref->flags, "flags mismatch");
    Z_HELPER_RUN(z_assert_ranges_eq(en->ranges, en->ranges_len,
                                    ref->ranges, ref->ranges_len),
                 "ranges mismatch");

    /* TODO Attributes. */
    /* TODO Aliases. */

    Z_HELPER_END;
}

static int z_assert_struct_eq(const iop_struct_t *st,
                              const iop_struct_t *ref);

static int z_assert_field_eq(const iop_field_t *f, const iop_field_t *ref)
{
    Z_ASSERT_LSTREQUAL(f->name, ref->name, "names mismatch");
    Z_ASSERT_EQ(f->tag, ref->tag, "tag mismatch");
    Z_ASSERT(f->tag_len == ref->tag_len, "tag_len field mismatch");
    Z_ASSERT(f->flags == ref->flags, "flags mismatch");
    Z_ASSERT_EQ(f->size, ref->size, "sizes mismatch");
    Z_ASSERT(f->type == ref->type, "types mismatch");
    Z_ASSERT(f->repeat == ref->repeat, "repeat field mismatch");
    Z_ASSERT_EQ(f->data_offs, ref->data_offs, "offset mismatch");

    /* TODO Check default value. */

    if (!iop_type_is_scalar(f->type)) {
        /* TODO Protect against loops. */
        Z_HELPER_RUN(z_assert_struct_eq(f->u1.st_desc, ref->u1.st_desc),
                     "struct type mismatch");
    } else
    if (f->type == IOP_T_ENUM) {
        Z_HELPER_RUN(z_assert_enum_eq(f->u1.en_desc, ref->u1.en_desc),
                     "enum type mismatch");
    }

    Z_HELPER_END;
}

/* Check that two IOP structs are identical. The name can differ. */
static int z_assert_struct_eq(const iop_struct_t *st,
                              const iop_struct_t *ref)
{
    if (st == ref) {
        return 0;
    }

    Z_ASSERT_EQ(st->fields_len, ref->fields_len);

    for (int i = 0; i < st->fields_len; i++) {
        const iop_field_t *fdesc = &st->fields[i];
        const iop_field_t *ref_fdesc = &ref->fields[i];

        Z_HELPER_RUN(z_assert_field_eq(fdesc, ref_fdesc),
                     "got difference(s) on field #%d (`%pL')",
                     i, &ref_fdesc->name);
    }

    Z_ASSERT(st->is_union == ref->is_union);
    Z_ASSERT(st->flags == ref->flags, "flags mismatch: %d vs %d",
             st->flags, ref->flags);
    /* TODO Check attributes. */

    Z_HELPER_RUN(z_assert_ranges_eq(st->ranges, st->ranges_len,
                                    ref->ranges, ref->ranges_len),
                 "ranges mismatch");

    Z_HELPER_END;
}

static int _test_struct(const char *pkg_file, int st_index,
                        const char **jsons, int nb_jsons,
                        const iop_struct_t *nullable ref_st_desc)
{
    t_scope;
    SB_1k(err);
    SB_1k(buf);
    iop_pkg_t *pkg;
    const iop_struct_t *st_desc;

    Z_HELPER_RUN(t_package_load(&pkg, pkg_file, LSTR_NULL_V));
    st_desc = pkg->structs[st_index];

    if (ref_st_desc) {
        Z_HELPER_RUN(z_assert_struct_eq(st_desc, ref_st_desc),
                     "struct description mismatch");
    }

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

    Z_HELPER_END;
}

#define test_struct(_file, _idx, st_desc, ...)                               \
    ({                                                                       \
        const char *__files[] = { __VA_ARGS__ };                             \
                                                                             \
        _test_struct(_file, _idx, __files, countof(__files), (st_desc));     \
    })

/* }}} */
/* {{{ Z_GROUP */

Z_GROUP_EXPORT(iopsq) {
    IOP_REGISTER_PACKAGES(&iopsq__pkg);
    IOP_REGISTER_PACKAGES(&tstiop__pkg);

    Z_TEST(struct_, "basic struct") {
        Z_HELPER_RUN(test_struct("struct.json", 0, NULL,
                                 "{\"i1\":42,\"i2\":2,\"s\":\"foo\"}"));
    } Z_TEST_END;

    Z_TEST(sub_struct, "struct with struct field") {
        t_scope;
        const char *v1 = "{\"i\":51}";
        const char *v2 = "{\"i\":12345678}";
        const char *tst1;
        const char *tst2;

        tst1 = t_fmt("{\"st\":%s,\"stRef\":%s}", v1, v2);
        tst2 = t_fmt("{\"st\":%s,\"stRef\":%s,\"stOpt\":%s}", v1, v2, v1);

        Z_HELPER_RUN(test_struct("sub-struct.json", 1, tstiop__s2__sp, tst1,
                                 tst2));
    } Z_TEST_END;

    Z_TEST(union_, "basic union") {
        Z_HELPER_RUN(test_struct("union.json", 0, NULL,
                                 "{\"i\":6}",
                                 "{\"s\":\"toto\"}"));
    } Z_TEST_END;

    Z_TEST(enum_, "basic enum") {
        Z_HELPER_RUN(test_struct("enum.json", 0, tstiop__iop_sq_enum_st__sp,
                                 "{\"en\":\"VAL1\"}",
                                 "{\"en\":\"VAL2\"}",
                                 "{\"en\":\"VAL3\"}"));
    } Z_TEST_END;

    Z_TEST(array, "array") {
        Z_HELPER_RUN(test_struct("array.json", 0, tstiop__array_test__sp,
                                 "{\"i\":[4,5,6]}"));
    } Z_TEST_END;

    Z_TEST(external_types, "external type names") {
        Z_HELPER_RUN(test_struct("external-types.json", 0,
                                 tstiop__test_external_types__sp,
                                 "{\"st\":{\"i\":42},\"en\":\"B\"}"));
    } Z_TEST_END;

    Z_TEST(error_unknown_type, "error case: unknown type name") {
        t_scope;
        iop_pkg_t *pkg;
        const char *err;

        err = "failed to resolve the package: "
              "error: unable to find any pkg providing type `Unknown`";

        Z_HELPER_RUN(t_package_load(&pkg, "error-unknown-type.json",
                                    LSTR(err)));
    } Z_TEST_END;

    Z_TEST(error_invalid_pkg_name, "error case: invalid package name") {
        SB_1k(err);
        static struct {
            const char *pkg_name;
            const char *jpack_err;
            const char *lib_err;
        } tests[] = {
            {
                "foo..bar", NULL,
                "invalid package `foo..bar': "
                "invalid name: empty package or sub-package name"
            }, {
                "fOo.bar",
                "1:9: invalid field (ending at `\"fOo.bar\"'): "
                "in type iopsq.Package: violation of constraint pattern "
                "([a-z_\\.]*) on field name: fOo.bar",
                NULL
            }, {
                "foo.", NULL,
                "invalid package `foo.': "
                "invalid name: trailing dot in package name"
            }
        };

        carray_for_each_ptr(t, tests) {
            t_scope;
            lstr_t json = t_build_json_pkg(t->pkg_name);
            pstream_t ps = ps_initlstr(&json);
            iop__package__t pkg_desc;
            int res;

            sb_reset(&err);
            res = t_iop_junpack_ps(&ps, iopsq__package__sp, &pkg_desc, 0,
                                   &err);
            if (t->jpack_err) {
                Z_ASSERT_STREQUAL(err.data, t->jpack_err);
                continue;
            }
            Z_ASSERT_N(res);
            Z_ASSERT_P(t->lib_err);
            Z_ASSERT_NULL(mp_iopsq_build_pkg(t_pool(), &pkg_desc, &err),
                          "unexpected success");
            Z_ASSERT_STREQUAL(err.data, t->lib_err);
        }
    } Z_TEST_END;

    Z_TEST(full_struct, "test with a struct as complete as possible") {
        t_scope;
        iop_pkg_t *pkg;
        const iop_struct_t *st;
        lstr_t st_name = LSTR("FullStruct");

        /* FIXME: some types cannot be implemented with IOP² yet (classes and
         * fields with default values) so we have to use types from tstiop to
         * avoid dissimilarities between structs. */
        Z_HELPER_RUN(t_package_load(&pkg, "full-struct.json", LSTR_NULL_V));
        st = iop_pkg_get_struct_by_name(pkg, st_name);
        Z_ASSERT_P(st, "cannot find struct `%pL'", &st_name);
        Z_HELPER_RUN(z_assert_struct_eq(st, &tstiop__full_struct__s),
                     "structs mismatch");
    } Z_TEST_END;

    Z_TEST(error_misc, "struct error cases miscellaneous") {
        t_scope;
        SB_1k(err);
        const iop__package__t *pkg_desc;
        const char *errors[] = {
            /* TODO Detect the bad type name instead. */
            "failed to resolve the package: error: "
                "unable to find any pkg providing type `foo..Bar`",
            "invalid package `user_package': "
                "cannot load `MultiDimensionArray': field `multiArray': "
                "multi-dimension arrays are not supported",
            "invalid package `user_package': "
                "cannot load `OptionalArray': field `optionalArray': "
                "repeated field cannot be optional or have a default value",
            "invalid package `user_package': "
                "cannot load `OptionalReference': field `optionalReference': "
                "optional references are not supported",
            "invalid package `user_package': "
                "cannot load `ArrayOfReference': field `arrayOfReference': "
                "arrays of references are not supported",
            "invalid package `user_package': "
                "cannot load `TagConflict': field `f2': "
                "tag `42' is already used by field `f1'",
            "invalid package `user_package': "
                "cannot load `NameConflict': field `field': "
                "name already used by another field",
            "invalid package `user_package': "
                "cannot load enum `ValueConflict': "
                "key `B': the value `42' is already used",
            "invalid package `user_package': "
                "cannot load enum `KeyConflict': "
                "the key `A' is duplicated",
        };
        const char **exp_error = errors;

        pkg_desc = t_load_package_from_file("error-misc.json", &err);
        Z_ASSERT_P(pkg_desc, "%pL", &err);
        Z_ASSERT_EQ(pkg_desc->elems.len, countof(errors));

        tab_for_each_entry(elem, &pkg_desc->elems) {
            t_scope;

            Z_ASSERT_NULL(mp_iopsq_build_mono_element_pkg(t_pool(), elem,
                                                          &err),
                          "unexpected success for struct %*pS "
                          "(expected error: %s)",
                          IOP_OBJ_FMT_ARG(elem), *exp_error);
            Z_ASSERT_STREQUAL(err.data, *exp_error,
                              "unexpected error message");
            exp_error++;
        }
    } Z_TEST_END;
} Z_GROUP_END;

/* }}} */

int main(int argc, char **argv)
{
    z_setup(argc, argv);
    z_register_exports(PLATFORM_PATH LIBCOMMON_PATH "iopc/");
    return z_run();
}
