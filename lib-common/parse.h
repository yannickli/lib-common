/* general things about parsing functions :


signed_int_type
`header'_parse_`what_it_parse' (const blob_t * blob, ssize_t * pos, foo ** answer)
 
Arguments :
   in case of successful parse, pos is positionned at the octet just after the
   last parsed octet, or after NULs in case of c-str-like tokens.

   in case of error, pos, answer and other function-modified arguments are never
   modified.

   output variable answer has to point on a valid *and* initialized value.

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

#ifndef IS_PARSE_H
#define IS_PARSE_H

enum blob_parse_status {
    PARSE_OK     = 0,
    PARSE_EPARSE = -1,
    PARSE_ERANGE = -2,
};

static inline bool blob_eop(blob_t * blob, ssize_t pos) {
    return blob->len == pos;
}


#endif
