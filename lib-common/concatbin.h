#ifndef CONCATBIN_H
#define CONCATBIN_H
#include <stdlib.h>
#include "macros.h"
/* The format of the archive is very simple :
 *
 * The archive contains blobs of known length.
 * Each blob is described by its length (in bytes), printed in ASCII and
 * followed by a "\n" :
 * N "\n" [DATA1 DATA2 DATA3 ... DATA-N] 
 */
typedef struct concatbin {
    const byte *start;
    const byte *cur;
    ssize_t len;
} concatbin;

concatbin *concatbin_new(const char *filename);
int concatbin_getnext(concatbin *ccb, const byte **data, int *len);
void concatbin_delete(concatbin **ccb);

#endif /* CONCATBIN_H */
