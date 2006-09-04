#ifndef IS_BLOB_PRIV_H
#define IS_BLOB_PRIV_H
/*
 * A blob has a vital invariant, making every parse function avoid
 * buffer read overflows: there is *always* a '\0' in the data at
 * position len, implying that size is always >= len+1
 */
typedef struct {
    /* public interface */
    ssize_t len;
    byte *data;

    /* private interface */
    byte *area;   /* originally allocated block */
    ssize_t size;  /* allocated size */
    byte initial[BLOB_INITIAL_SIZE];
} real_blob_t;

#endif
