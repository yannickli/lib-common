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

#ifndef IS_LIB_COMMON_CFG_H
#define IS_LIB_COMMON_CFG_H

/*
   cfg files are ini-like files with an extended format.

  - leading and trailing spaces aren't significant.
  - any line can be wrapped with a backslash (`\`) as a continuation token.
  - quoted strings can embed usual C escapes (\a \b \n ...), octal chars
    (\ooo) and hexadecimal ones (\x??) and unicode ones (\u????).

  Encoding should be utf-8.

----8<----

[simple]
key = value

[section "With a Name"]
key = 1234
other = "some string with embeded spaces"
; comment
# alternate comment form
foo = /some/value/without[spaces|semicolon|dash]

[section "With a very very way too long Name to show the \
line splitting feature, but beware, spaces after a continuation are \
significant"]


---->8----
 */

#include "blob.h"
#include "string_is.h"

int cfg_parse(const char *file);

#endif /* IS_LIB_COMMON_CFG_H */
