/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_PARSE_H
#define IS_LIB_COMMON_PARSE_H

/* general things about parsing functions :

signed_int_type `header'_parse_`what_it_parses' (const blob_t * blob,
                                                 ssize_t * pos, foo ** answer)
 
Arguments :
   in case of successful parse, pos is positionned at the octet just
   after the last parsed octet, or after NULs in case of c-str-like tokens.

   in case of error, pos, answer and other function-modified arguments
   are never modified.

   output variable answer has to point on a valid *and* initialized value.

   if the pointer on the output variable is NULL, data is eaten.

Return values:
   
   negative return values are always an error.
   0 return value is always a success.
   positive return values are a success, and generally have a special meaning.

   parse functions that returns const char ** usualy return the strlen of the
   char **.

Notes:

   parse function assume pos is in the blob and must ensure that the returned
   pos is at most equal to blob->len

   a pos value equal to blob->len is legal and mean that end of pars has been
   reached.

 */

#include "blob.h"

enum blob_parse_status {
    PARSE_OK     = 0,
    PARSE_EPARSE = -1,
    PARSE_ERANGE = -2,
};

#define PARSE_SET_RESULT(var, value)        \
    do {                                    \
        if ((var) != NULL) {                \
            *(var) = (value);               \
        }                                   \
    } while(0)

#define TRANSMIT_PARSE_ERROR(result, expr)  \
    do {                                    \
        if (((result) = (expr)) < 0) {      \
            return result;                  \
        }                                   \
    } while(0)

#endif /* IS_LIB_COMMON_PARSE_H */
