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

#ifndef IS_LIB_COMMON_STR_ARRAY_H
#define IS_LIB_COMMON_STR_ARRAY_H

#include <lib-common/mem.h>
#include <lib-common/array.h>

// define our arrays
ARRAY_TYPE(char, string);
ARRAY_FUNCTIONS(char, string, p_delete)
void string_array_dump(const string_array *xp);

string_array *str_explode(const char *s, const char *tokens);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_str_array_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/

#endif /* IS_LIB_COMMON_STR_ARRAY_H */
