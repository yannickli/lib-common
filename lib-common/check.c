/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
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

#include "archive.h"
#include "bfield.h"
#include "blob.h"
#include "conf.h"
#include "fifo.h"
#include "licence.h"
#include "log_limit.h"
#include "str_array.h"
#include "string_is.h"
#include "strconv.h"
#include "timeval.h"
#include "timer.h"
#include "xml.h"

int main(void)
{
    int nf;
    SRunner *sr = srunner_create(NULL);

    srunner_add_suite(sr, check_string_is_suite());
    srunner_add_suite(sr, check_make_strconv_suite());
    srunner_add_suite(sr, check_append_blob_ebcdic_suite(
            check_make_blob_suite()
            ));
    srunner_add_suite(sr, check_make_archive_suite());
    srunner_add_suite(sr, check_fifo_suite());
    srunner_add_suite(sr, check_licence_suite());
    srunner_add_suite(sr, check_xml_suite());
    srunner_add_suite(sr, check_make_timeval_suite());
    srunner_add_suite(sr, check_bfield_suite());
    srunner_add_suite(sr, check_str_array_suite());
    srunner_add_suite(sr, check_conf_suite());
    srunner_add_suite(sr, check_log_limit_suite());
    srunner_add_suite(sr, check_timer_suite());

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
