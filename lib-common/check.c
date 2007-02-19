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

#include <check.h>
#include <stdlib.h>

#include "blob.h"
#include "string_is.h"
#include "msg_template.h"
#include "fifo.h"
#include "archive.h"
#include "licence.h"
#include "xml.h"
#include "timeval.h"

int main(void)
{
    int nf;
    SRunner *sr = srunner_create(NULL);

    srunner_add_suite(sr, check_string_is_suite());
    srunner_add_suite(sr, check_make_blob_suite());
    srunner_add_suite(sr, check_make_archive_suite());
    srunner_add_suite(sr, check_msg_template_suite());
    srunner_add_suite(sr, check_fifo_suite());
    srunner_add_suite(sr, check_licence_suite());
    srunner_add_suite(sr, check_xml_suite());
    srunner_add_suite(sr, check_make_timeval_suite());

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
