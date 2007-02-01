/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <string.h>
#include <stdio.h>

#include "blob.h"

static int check_aiconv_templ(const char *file1, const char *file2,
                              const char *encoding)
{
    blob_t b1;
    blob_t b2;
    blob_t b3;
    
    int c_typ = 0;

    blob_init(&b1);
    blob_init(&b2);
    blob_init(&b3);
    
    blob_file_auto_iconv(&b2, file1, encoding, &c_typ);

    blob_append_file_data(&b3, file2);  
    if (blob_cmp(&b2, &b3) != 0)
        printf("blob_auto_iconv failed on: %s with" \
               " hint \"%s\" encoding\n", file1, encoding);
    
    blob_wipe(&b1);
    blob_wipe(&b2);
    blob_wipe(&b3); 
    
    return 0;
}

static int check_aiconv_templ_2(const char *file1, const char *file2)
{
    check_aiconv_templ(file1, file2, "UTF-8");
    check_aiconv_templ(file1, file2, "ISO-8859-1");
    check_aiconv_templ(file1, file2, "Windows-1250");
    
    return 0;
}

START_TEST(check_blob_iconv_close)
{
    blob_t b1;
    blob_t b2;
    int c_typ = 0;
    int n;

    blob_init(&b1);
    blob_init(&b2);

    blob_file_auto_iconv(&b2, "samples/example1.latin1", "ISO-8859-1",
                         &c_typ);
    blob_file_auto_iconv(&b2, "samples/example1.utf8", "UTF-8",
                         &c_typ);
    blob_file_auto_iconv(&b2, "samples/example2.windows-1250", "windows-1250",
                         &c_typ);

    n = blob_iconv_close_all();
    fail_if(n > 2,
            "blob_file_auto_iconv allocated %d iconv_t handles instead of %d",
            n, 2);

    blob_wipe(&b1);
    blob_wipe(&b2);
}
END_TEST

START_TEST(check_blob_auto_iconv)
{
    check_aiconv_templ_2("samples/example1.latin1",
                         "samples/example1.utf8");
    check_aiconv_templ_2("samples/example2.windows-1250",
                         "samples/example2.utf8");
    check_aiconv_templ_2("samples/example1.utf8",
                         "samples/example1.utf8");
    check_aiconv_templ("samples/example3.windows-1256",
                       "samples/example3.utf8",
                       "Windows-1256");
    check_aiconv_templ_2("samples/example4.latin1",
                         "samples/example4.utf8");

    blob_iconv_close_all();
}
END_TEST

static Suite *check_make_blob_iconv_suite(void)
{
    Suite *s  = suite_create("Blob");
    TCase *tc = tcase_create("Iconv");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_blob_iconv_close);
    tcase_add_test(tc, check_blob_auto_iconv);

    return s;
}

int main(void)
{
    int nf;
    SRunner *sr = srunner_create(NULL);

    srunner_add_suite(sr, check_make_blob_iconv_suite());
    
    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
