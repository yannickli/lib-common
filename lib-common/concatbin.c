#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <lib-common/string_is.h>
#include "err_report.h"
#include "macros.h"
#include "mem.h"
#include "concatbin.h"

concatbin *concatbin_new(const char *filename)
{
    concatbin *ccb = NULL;
    struct stat sbuf;
    int fd = -1;

    ccb = p_new(concatbin, 1);

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        e_error("open failed\n");
        goto error;
    }

    if (fstat(fd, &sbuf)) {
        e_error( "fstat failed\n");
        goto error;
    }

    ccb->len = sbuf.st_size;
    ccb->start = mmap(NULL, ccb->len, PROT_READ, MAP_PRIVATE, fd, 0);

    if (ccb->start == MAP_FAILED) {
        e_error( "mmap failed\n");
        goto error;
    }
    
    ccb->cur = ccb->start;
    return ccb;
error:
    if (fd >= 0) {
        close(fd);
    }
    if (ccb) {
        p_delete(&ccb);
    }
    return NULL;
}

int concatbin_getnext(concatbin *ccb, const byte **data, int *len)
{
    long len1;
    if (!ccb || !data || !len) {
        return -1;
    }
#if 0
    fprintf(stderr, "getnext: ccb[start:%p len:%zd cur:%p]\n",
            ccb->start, ccb->len, ccb->cur);
#endif

    if (ccb->cur == ccb->start + ccb->len) {
        /* End reached */
        return 1;
    }
    len1 = strtol((const char*)ccb->cur, (const char**)&ccb->cur, 10);
    if (len1 <= 0 || len1 > INT_MAX) { /* LONG_INT < 0, LONG_MAX > INT_MAX */
        goto error;
    }
    *len = len1;
    if (*ccb->cur != '\n') {
        e_debug(1, "'%c' is not a CR\n", *ccb->cur);
        goto error;
    }
    ccb->cur++; /* skip \n */
    *data = ccb->cur;
    ccb->cur += *len; /* get ready on next part */
    
    if (ccb->cur > ccb->start + ccb->len) {
        e_debug(1, "Past the end\n");
        goto error;
    }
    return 0;
error:
    *len = 0;
    *data = NULL;
    return -1;
}

void concatbin_delete(concatbin **ccb)
{
    if (!ccb || !*ccb) {
        return;
    }
    munmap((void *)(*ccb)->start, (*ccb)->len);
    p_delete(ccb);
}
