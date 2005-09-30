#include <check.h>
#include <stdlib.h>

#include "blob.h"
#include "string_is.h"

int main(void)
{
    int nf;
    SRunner * sr = srunner_create(NULL);

    srunner_add_suite(sr, check_string_is_suite());
    srunner_add_suite(sr, check_make_blob_suite());

    srunner_run_all(sr, CK_NORMAL);
    nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
