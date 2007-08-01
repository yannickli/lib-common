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

#include "str_array.h"

void string_array_dump(const string_array *xp)
{
    const char *sep;
    int i;

    sep = "";
    for (i = 0; i < xp->len; i++) {
        printf("%s%s", sep, xp->tab[i]);
        sep = " ";
    }
}

/* OG: API discussion:
 * str_explode() as a reference to PHP's explode() function but with
 * different semantics: order of parameters is reversed, and delimiter
 * is en actual string, not a set a characters.
 * We could also name this str_split() as a reference to Javascript's
 * String split method.
 * - a NULL token string could mean a standard set of delimiters
 * - an empty token string will produce a singleton
 * - an empty string will produce a NULL instead of an empty array ?
 */

string_array *str_explode(const char *s, const char *tokens)
{
    const char *p;
    string_array *res;

    if (!s || !tokens || !*s) {
        return NULL;
    }

    res = string_array_new();
    p = strpbrk(s, tokens);

    while (p != NULL) {
        string_array_append(res, p_dupstr(s, p - s));
        s = p + 1;
        p = strchr(s, *p);
    }

    /* Last part */
    if (*s) {
        string_array_append(res, p_strdup(s));
    }
    return res;
}


/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

START_TEST(check_str_explode)
{
    string_array *arr;

#define STR_EXPL_CORRECT_TEST(str1, str2, str3, sep)                       \
    arr = str_explode(str1 sep str2 sep str3, sep);                        \
                                                                           \
    fail_if(arr == NULL, "str_explode failed, res == NULL");               \
    fail_if(arr->len != 3, "str_explode failed: len = %zd != 3", arr->len); \
    fail_if(strcmp(arr->tab[0], str1),                                     \
            "str_explode failed: tab[0] (%s) != %s",                       \
            arr->tab[0], str1);                                            \
    fail_if(strcmp(arr->tab[1], str2),                                     \
            "str_explode failed: tab[1] (%s) != %s",                       \
            arr->tab[1], str2);                                            \
    fail_if(strcmp(arr->tab[2], str3),                                     \
            "str_explode failed: tab[2] (%s) != %s",                       \
            arr->tab[2], str3);                                            \
                                                                           \
    string_array_delete(&arr);
    STR_EXPL_CORRECT_TEST("123", "abc", "!%*", "/");
    STR_EXPL_CORRECT_TEST("123", "abc", "!%*", " ");
    STR_EXPL_CORRECT_TEST("123", "abc", "!%*", "$");
    STR_EXPL_CORRECT_TEST("   ", ":::", "!!!", ",");

    arr = str_explode("secret1 secret2 secret3", " ,;");
    fail_if(arr == NULL, "str_explode failed, res == NULL");
    fail_if(arr->len != 3, "str_explode failed: len = %zd != 3", arr->len);
    string_array_delete(&arr);
    arr = str_explode("secret1;secret2;secret3", " ,;");
    fail_if(arr == NULL, "str_explode failed, res == NULL");
    fail_if(arr->len != 3, "str_explode failed: len = %zd != 3", arr->len);
    string_array_delete(&arr);
    arr = str_explode("secret1,secret2 secret3", " ,;");
    fail_if(arr == NULL, "str_explode failed, res == NULL");
    fail_if(arr->len != 2, "str_explode failed: len = %zd != 2", arr->len);
    string_array_delete(&arr);
}
END_TEST

Suite *check_str_array_suite(void)
{
    Suite *s  = suite_create("str_array");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_str_explode);
    return s;
}
#endif

