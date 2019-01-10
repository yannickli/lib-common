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

#ifndef IS_LIB_COMMON_PAGED_INDEX_H
#define IS_LIB_COMMON_PAGED_INDEX_H

#include "mmappedfile.h"

typedef struct pidx_page {
    union {
        struct {
            int32_t next;
            byte payload[1024 - 4];
        };
        int32_t refs[256];
    };
} pidx_page;

#define PIDX_PAGE         ssizeof(pidx_page)
#define PIDX_PAYLOAD_SIZE (PIDX_PAGE - 2 * ssizeof(int32_t))
#define PIDX_MKVER(x, y)  (((x) << 8) | (y))
#define PIDX_MAJOR        1
#define PIDX_MINOR        0
#define PIDX_VERSION      PIDX_MKVER(PIDX_MAJOR, PIDX_MINOR)

/** \brief this struct is the header of an intersec paginated file.
 * Such a file is stored in 64-bit aligned big endian structures,
 * like what GCC chooses on the x86_64 architecture.
 *
 * \warning the current implementation has limitations:
 *   \li it does not work on little endian archs.
 *   \li the number of segments cannot be greater than 6.
 */
typedef struct pidx_t {
    /* first qword */
    uint32_t magic;     /**< magic of the paginated file: 'I' 'S' 'P' 'F'. */
    uint8_t  major;     /**< major version of the file format              */
    uint8_t  minor;     /**< minor version of the file format              */
    uint8_t  skip;      /**< number of bits that must be zero for indexes  */
    uint8_t  nbsegs;    /**< number of 10-bits chunks for the pagination   */

    /* second qword */
    int32_t nbpages;    /**< number of allocated pages in the file         */
    int32_t freelist;   /**< freelist of blank pages                       */

    /* third qword */
    int16_t  wrlock;    /**< holds the pid of the writer if any.           */
    int16_t  reserved1;
    int32_t  reserved2;

    /* fourth qword */
    uint64_t wrlockt;   /**< time associated to the lock                   */

    /* __future__: 128 - 4 qwords */
    uint64_t reserved[64 - 4]; /**< padding up to 2k                       */

    uint64_t subhdr[64];       /**< reserved for hosted file headers: 2k   */

    /** \brief pages of the @pidx_t.
     * pages[0] is _always_ the first level of pagination.
     * pages[0 .. nbpages - 1] shall be accessible.
     */
    pidx_page pages[];
} pidx_t;

typedef struct pidx_file MMFILE_ALIAS(pidx_t) pidx_file;

/****************************************************************************/
/* whole file related functions                                             */
/****************************************************************************/

__must_check__ pidx_file *
pidx_open(const char *path, int flags, uint8_t skip, uint8_t nbsegs);
void pidx_close(pidx_file **f);

int pidx_clone(pidx_file *pidx, const char *filename);

/****************************************************************************/
/* low level keys related functions                                         */
/****************************************************************************/

int pidx_key_first(pidx_file *pidx, uint64_t minval, uint64_t *res);
int pidx_key_last(pidx_file *pidx, uint64_t maxval, uint64_t *res);

static inline int
pidx_key_next(pidx_file *pidx, uint64_t cur, uint64_t *res) {
    if (cur == UINT64_MAX)
        return -1;
    return pidx_key_first(pidx, cur + 1, res);
}

static inline int
pidx_key_prev(pidx_file *pidx, uint64_t cur, uint64_t *res) {
    if (cur == 0)
        return -1;
    return pidx_key_last(pidx, cur - 1, res);
}

/****************************************************************************/
/* high level functions                                                     */
/****************************************************************************/

int pidx_data_get(pidx_file *pidx, uint64_t idx, sb_t *out);

int pidx_data_getslice(pidx_file *pidx, uint64_t idx,
                       byte *out, int start, int len) __must_check__;

int pidx_data_set(pidx_file *pidx, uint64_t idx, const byte *data, int len);
void pidx_data_release(pidx_file *pidx, uint64_t idx);


/* XXX unsafe wrt threads and locks */
void *pidx_data_getslicep(pidx_file *pidx, uint64_t idx,
                          int start, int len);
#endif
