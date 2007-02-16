/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
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

#include <stdint.h>
#include <lib-common/macros.h>
#include <lib-common/blob.h>
#include <lib-common/mmappedfile.h>

typedef struct pidx_page {
    union {
        struct {
            int32_t next;
            byte payload[];
        };
        int32_t refs[256];
    };
} pidx_page;

#define PIDX_PAGE  ssizeof(pidx_page)

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

    /* __future__: 128 - 2 qwords */
    uint64_t reserved[64 - 2]; /**< padding up to 2k                      */

    uint64_t subhdr[64];       /**< reserved for hosted file headers: 2k  */

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

pidx_file *pidx_open(const char *path, int flags);
pidx_file *pidx_creat(const char *path, uint8_t skip, uint8_t nbsegs);
void pidx_close(pidx_file **f);

/** \brief checks and repair idx files.
 * \param    pidx     the paginated index file to check/fix.
 * \param    dofix
 *     what shall be done with @pidx:
 *         - 0 means check only.
 *         - 1 means fix if necessary.
 *         - 2 means assume broken and fix.
 * \return
 *   - 0 if check is sucessful.
 *   - 1 if the file was fixed (modified) but that the result is a valid file.
 *   - -1 if the check failed and that either the file is not fixable or that
 *        fixing it was not allowed.
 */
int pidx_fsck(pidx_file *pidx, int dofix);


/****************************************************************************/
/* low level page related functions                                         */
/****************************************************************************/

int32_t pidx_page_find(pidx_file *pidx, uint64_t idx);
int32_t pidx_page_new(pidx_file *pidx, uint64_t idx);


/****************************************************************************/
/* high functions                                                           */
/****************************************************************************/

int pidx_data_get(pidx_file *pidx, uint64_t idx, blob_t *out);
int pidx_data_set(pidx_file *pidx, uint64_t idx, const byte *data, int len);
void pidx_data_release(pidx_file *pidx, uint64_t idx);

#endif
