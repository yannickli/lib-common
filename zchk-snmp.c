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

/* LCOV_EXCL_START */

#include "iop-snmp.h"
#include "z.h"

#include "test-data/snmp/snmp_test.iop.h"
#include "test-data/snmp/snmp_test_doc.iop.h"
#include "test-data/snmp/snmp_intersec_test.iop.h"

/* {{{ MIB tests */

static qv_t(mib_rev) t_z_fill_up_revisions(void)
{
    qv_t(mib_rev) revisions;

    t_qv_init(&revisions, PATH_MAX);
    mib_register_revision(&revisions, "201003091349Z", "Initial release");

    return revisions;
}

static void z_init(qv_t(pkg) *pkgs)
{
    qv_init(pkgs);
}

static void z_wipe(qv_t(pkg) pkgs)
{
    qv_wipe(&pkgs);
}

static int z_check_wanted_file(const char *filename, sb_t *sb)
{
    char path[PATH_MAX];
    lstr_t file_map;

    snprintf(path, sizeof(path), "%*pM/%s",
             LSTR_FMT_ARG(z_cmddir_g), filename);

    Z_ASSERT_N(lstr_init_from_file(&file_map, path, PROT_READ, MAP_SHARED));

    Z_ASSERT_LSTREQUAL(file_map, LSTR_SB_V(sb));

    lstr_wipe(&file_map);
    Z_HELPER_END;
}

Z_GROUP_EXPORT(iop_snmp_mib)
{
    Z_TEST(test_intersec_mib_generated, "compare generated and ref file") {
        t_scope;
        SB_8k(sb);
        const char *ref_file = "test-data/snmp/mibs/REF-INTERSEC-MIB.txt";
        qv_t(mib_rev) revisions = t_z_fill_up_revisions();
        qv_t(pkg) pkgs;

        z_init(&pkgs);

        qv_append(&pkgs, &snmp_intersec_test__pkg);
        iop_write_mib(&sb, &pkgs, &revisions);

        Z_HELPER_RUN(z_check_wanted_file(ref_file, &sb));

        z_wipe(pkgs);
    } Z_TEST_END;

    Z_TEST(test_intersec_mib_smilint, "test intersec mib using smilint") {
        t_scope;
        SB_8k(sb);
        qv_t(mib_rev) revisions = t_z_fill_up_revisions();
        qv_t(pkg) pkgs;
        char *path = t_fmt("%*pM/intersec", LSTR_FMT_ARG(z_tmpdir_g));
        lstr_t cmd;

        z_init(&pkgs);

        qv_append(&pkgs, &snmp_intersec_test__pkg);
        iop_write_mib(&sb, &pkgs, &revisions);

        /* Check smilint compliance level 6 */
        sb_write_file(&sb, path);
        cmd = t_lstr_fmt("smilint -s -e -l 6 %s", path);
        Z_ASSERT_ZERO(system(cmd.s));

        z_wipe(pkgs);
    } Z_TEST_END;

    Z_TEST(test_entire_mib, "test complete mib") {
        t_scope;
        SB_8k(sb);
        qv_t(mib_rev) revisions = t_z_fill_up_revisions();
        qv_t(pkg) pkgs;
        const char *ref_file = "test-data/snmp/mibs/REF-TEST-MIB.txt";
        char *new_path = t_fmt("%*pM/tst", LSTR_FMT_ARG(z_tmpdir_g));
        lstr_t cmd;

        z_init(&pkgs);

        qv_append(&pkgs, &snmp_test__pkg);

        iop_write_mib(&sb, &pkgs, &revisions);
        Z_HELPER_RUN(z_check_wanted_file(ref_file, &sb));

        /* Check smilint compliance level 6 */
        sb_write_file(&sb, new_path);
        cmd = t_lstr_fmt("smilint -s -e -l 6 -i notification-not-reversible "
                         "-p %s %s",
                         "test-data/snmp/mibs/REF-INTERSEC-MIB.txt",
                         new_path);
        Z_ASSERT_ZERO(system(cmd.s));

        z_wipe(pkgs);
    } Z_TEST_END;

} Z_GROUP_END;

/* }}} */
/* {{{ SNMP-doc tests */

Z_GROUP_EXPORT(iop_snmp_doc)
{
    Z_TEST(test_doc, "test generated doc") {
        const char *ref_notif_file = "test-data/snmp/docs/ref-notif.inc.adoc";
        const char *ref_obj_file = "test-data/snmp/docs/ref-object.inc.adoc";
        SB_1k(notifs_sb);
        SB_1k(objects_sb);
        qv_t(pkg) pkgs;

        qv_init(&pkgs);

        qv_append(&pkgs, &snmp_test_doc__pkg);
        iop_write_snmp_doc(&notifs_sb, &objects_sb, &pkgs);

        Z_HELPER_RUN(z_check_wanted_file(ref_notif_file, &notifs_sb));
        Z_HELPER_RUN(z_check_wanted_file(ref_obj_file, &objects_sb));

        qv_wipe(&pkgs);
    } Z_TEST_END;

} Z_GROUP_END;

/* LCOV_EXCL_STOP */

/* }}} */

/* LCOV_EXCL_STOP */
