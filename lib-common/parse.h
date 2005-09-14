/* general things about parsing functions :
 
Arguments :
   in case of successful parse, pos is positionned at the octet just after the
   last parsed octet, or after NULs in case of c-str-like tokens.

   in case of error, pos, answer and other function-modified arguments are never
   modified.

Return values:
   
   negative return values are always an error.
   0 return value is always a success.
   positive return values are a success, and generally have a special meaning.

Notes:

   parse function assume pos is in the blob.
   though, they may return a pos that is equal to blob->len, meaning that the
   parse has reach its end.
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
