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
