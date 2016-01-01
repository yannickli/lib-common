/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2016 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_STR_BUF_PP_H
#define IS_LIB_COMMON_STR_BUF_PP_H

#include "core.h"
#include "container-qvector.h"

/** \file str-buf-pp.h
 * Helpers to perform some complex pretty-printing.
 */

/** Description of a column table.
 */
typedef struct table_hdr_t {
    /** Title of the column.
     */
    lstr_t title;

    /** Value to put if a cell is empty or missing.
     */
    lstr_t empty_value;

    /** Maximum width of the column.
     */
    int max_width;

    /** Minimum width of the column.
     */
    int min_width;

    /** Alignment of the column.
     */
    enum {
        ALIGN_LEFT,
        ALIGN_CENTER,
        ALIGN_RIGHT,
    } align;

    /** If true, add ellipsis (…) when the content does not fit in the maximum
     * width.
     */
    bool add_ellipsis;

    /** Omit the column if no value is found.
     */
    bool omit_if_empty;
} table_hdr_t;

qvector_t(table_hdr, table_hdr_t);
qvector_t(table_data, qv_t(lstr));

/** Format a table.
 *
 * This function appends a table formatted from the given columns, whose
 * descriptions are provided in \p hdr, and rows whose content is provided in
 * \p data in the buffer \p out. The content is guaranteed to end with a
 * newline character.
 *
 * The output contains a first row with the column title, followed by one line
 * per entry of \p data. The width of the columns is adjusted to their content
 * as well as the dimensioning parameters provided in the column description.
 * Columns are separated by two spaces. A row may contain less columns than
 * the header, in which case the missing cells are filled with the default
 * values for those columns.
 *
 * The header of the columns is always left-aligned. The last column may
 * contain extra data that does not fit on a single line.
 *
 * \param[in] out  The output buffer.
 * \param[in] hdr  The description of the columns.
 * \param[in] data The content of the table.
 */
void sb_add_table(sb_t *out, const qv_t(table_hdr) *hdr,
                  const qv_t(table_data) *data);

#endif /* IS_LIB_COMMON_STR_BUF_PP_H */
