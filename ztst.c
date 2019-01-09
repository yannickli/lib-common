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

#include <check.h>
#include "all.h"
#include "blob.h"

int main(void)
{
    int nf;
    SRunner *sr = srunner_create(NULL);

    srunner_add_suite(sr, check_string_suite());
    srunner_add_suite(sr, check_make_strconv_suite());
    srunner_add_suite(sr,
            check_append_blob_wbxml_suite(
            check_append_blob_ebcdic_suite(
            check_make_blob_suite()
    )));
#if 0 /* in mcms now */
    srunner_add_suite(sr, check_make_archive_suite());
#endif
    srunner_add_suite(sr, check_licence_suite());
    srunner_add_suite(sr, check_xml_suite());
    srunner_add_suite(sr, check_make_timeval_suite());
    srunner_add_suite(sr, check_bfield_suite());
    srunner_add_suite(sr, check_str_array_suite());
#if 0 /* in mcg now */
    srunner_add_suite(sr, check_range_vector_suite());
#endif
    srunner_add_suite(sr, check_conf_suite());

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
