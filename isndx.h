/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_ISNDX_H
#define IS_LIB_COMMON_ISNDX_H

#include "mmappedfile.h"

typedef struct isndx_t isndx_t;

typedef struct isndx_file_t MMFILE_ALIAS(struct isndx_file) isndx_file_t;

MMFILE_FUNCTIONS(isndx_file_t, isndx_real);

#define ISNDX_MAGIC       "ISGX"
#define ISNDX_MAJOR       1
#define ISNDX_MINOR       0
#define ISNDX_PAGESHIFT   10
#define ISNDX_PAGESIZE    (1 << ISNDX_PAGESHIFT)

#define ROUND_SHIFT(x,s)  (((x) + (1 << (s)) - 1) & ~((1 << (s)) - 1))
#define NB_PAGES_GROW     32

#define MAX_KEYLEN        255
#define MAX_DATALEN       255
#define MAX_DEPTH         16

#define O_ISWRITE(m)      (((m) & (O_RDONLY|O_WRONLY|O_RDWR)) != O_RDONLY)

struct isndx_file {
    byte magic[4];
    uint32_t major;       /* isndx internal version */
    uint32_t minor;       /* isndx internal version */
    uint32_t pageshift;
    uint32_t pagesize;
    uint32_t root;
    int32_t rootlevel;
    uint32_t nbpages;
    uint32_t nbkeys;
    int32_t minkeylen;
    int32_t maxkeylen;
    int32_t mindatalen;
    int32_t maxdatalen;
    uint32_t user_major;  /* user private version */
    uint32_t user_minor;  /* user private version */
};

struct isndx_t {
    isndx_file_t *file;
    uint32_t pageshift;
    uint32_t pagesize;
    uint32_t root;
    int32_t rootlevel;
    uint32_t npages, nkeys;     /* used by isndx_check() */
    int error_code;
    FILE *error_stream;
    char error_buf[128];
};


typedef struct isndx_create_parms_t {
    int flags;
    int pageshift;
    int minkeylen, maxkeylen;
    int mindatalen, maxdatalen;
} isndx_create_parms_t;

isndx_t *isndx_create(const char *path, isndx_create_parms_t *cp);

isndx_t *isndx_open(const char *path, int flags);
void isndx_close(isndx_t **ndx);

int isndx_fetch(isndx_t *ndx, const byte *key, int klen, sb_t *out);
int isndx_push(isndx_t *ndx, const byte *key, int klen,
               const void *data, int len);

static inline int isndx_fetch_uint64(isndx_t *ndx, uint64_t key, sb_t *out)
{
    return isndx_fetch(ndx, (const byte *)&key, sizeof(key), out);
}
static inline int isndx_push_uint64(isndx_t *ndx, uint64_t key, const void *data, int len)
{
    return isndx_push(ndx, (const byte *)&key, sizeof(key), data, len);
}

#define ISNDX_CHECK_ALL          1
#define ISNDX_CHECK_PAGES        2
#define ISNDX_CHECK_KEYS         4
#define ISNDX_CHECK_ISRIGHTMOST  8

int isndx_check(isndx_t *ndx, int flags);
int isndx_check_page(isndx_t *ndx, uint32_t pageno, int level,
                     int upkeylen, const byte *upkey, int flags);

#define ISNDX_DUMP_ALL        1
#define ISNDX_DUMP_PAGES      2
#define ISNDX_DUMP_KEYS       4
#define ISNDX_DUMP_ENUMERATE  8

void isndx_dump(isndx_t *ndx, int flags, FILE *fp);
void isndx_dump_page(isndx_t *ndx, uint32_t pageno, int flags, FILE *fp);

int isndx_get_error(isndx_t *ndx, char *buf, int size);
FILE *isndx_set_error_stream(isndx_t *ndx, FILE *stream);

int isndx_enumerate(isndx_t *ndx, void *opaque,
                    int (*cb)(void *opaque, isndx_t *ndx,
                              const byte *key, int klen,
                              const void *data, int len));

#endif /* IS_LIB_COMMON_ISNDX_H */
