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

#include "str-buf-pp.h"

/* {{{ Table printer */

static void sb_add_cell(sb_t *out, const struct table_hdr_t *col,
                        int col_size, bool is_hdr, bool is_last,
                        lstr_t content)
{
    int len = lstr_utf8_strlen(content);

    if (len > col_size && col->add_ellipsis) {
        content = lstr_utf8_truncate(content, col_size - 1);
        sb_add(out, content.s, content.len);
        sb_adduc(out, 0x2026 /* … */);
    } else
    if (len >= col_size) {
        content = lstr_utf8_truncate(content, col_size);
        sb_add(out, content.s, content.len);
    } else {
        int padding = col_size - len;
        int left_padding = 0;
        int right_padding = 0;

        switch (is_hdr ? ALIGN_LEFT : col->align) {
          case ALIGN_LEFT:
            break;

          case ALIGN_CENTER:
            left_padding = padding / 2;
            break;

          case ALIGN_RIGHT:
            left_padding = padding;
            break;
        }
        right_padding = padding - left_padding;
        if (left_padding) {
            sb_addnc(out, left_padding, ' ');
        }
        sb_add_lstr(out, content);
        if (right_padding && !is_last) {
            sb_addnc(out, right_padding, ' ');
        }
    }
}

/* Return 0 if something has been written. */
static int
sb_write_table_cell(sb_t *out, const struct table_hdr_t *col, int col_size,
                    bool is_hdr, bool is_first, bool is_last, lstr_t content,
                    int csv_sep)
{
    if (col_size == 0) {
        /* Omit column. */
        return -1;
    }

    if (!content.len) {
        content = col->empty_value;
    }

    if (!is_first) {
        if (csv_sep) {
            sb_addc(out, csv_sep);
        } else {
            sb_adds(out, "  ");
        }
    }

    if (csv_sep) {
        sb_add_lstr_csvescape(out, csv_sep, content);
    } else {
        sb_add_cell(out, col, col_size, is_hdr, is_last, content);
    }

    return 0;
}

/** Write table or a csv if csv_sep != 0. */
static void
sb_write_table(sb_t *out, const qv_t(table_hdr) *hdr,
               const qv_t(table_data) *data, int *col_sizes, int csv_sep)
{
    bool first_column = true;

    /* Write the header. */
    tab_for_each_pos(pos, hdr) {
        const struct table_hdr_t *column = &hdr->tab[pos];

        if (sb_write_table_cell(out, column, col_sizes[pos], true,
                                first_column, pos == hdr->len - 1,
                                column->title, csv_sep) == 0)
        {
            first_column = false;
        }
    }
    sb_addc(out, '\n');

    /* Write the content. */
    tab_for_each_ptr(row, data) {
        first_column = true;

        tab_for_each_pos(pos, hdr) {
            lstr_t content = LSTR_NULL;

            if (pos < row->len) {
                content = row->tab[pos];
            }

            if (sb_write_table_cell(out, &hdr->tab[pos], col_sizes[pos],
                                    false, first_column, pos == hdr->len - 1,
                                    content, csv_sep) == 0)
            {
                first_column = false;
            }
        }
        sb_addc(out, '\n');
    }
}

void sb_add_table(sb_t *out, const qv_t(table_hdr) *hdr,
                  const qv_t(table_data) *data)
{
    int *col_sizes = p_alloca(int, hdr->len);
    int row_size = 0;
    int col_count = 0;

    /* Compute the size of the columns */
    tab_for_each_pos(pos, hdr) {
        int *col = &col_sizes[pos];
        bool has_value = false;

        *col = MAX(hdr->tab[pos].min_width,
                   lstr_utf8_strlen(hdr->tab[pos].title));

        tab_for_each_ptr(row, data) {
            if (row->len <= pos) {
                *col = MAX(*col, lstr_utf8_strlen(hdr->tab[pos].empty_value));
            } else {
                *col = MAX(*col, lstr_utf8_strlen(row->tab[pos]));

                if (row->tab[pos].len) {
                    has_value = true;
                }
            }
        }

        if (hdr->tab[pos].max_width) {
            *col = MIN(*col, hdr->tab[pos].max_width);
        }
        if (hdr->tab[pos].omit_if_empty && !has_value) {
            *col = 0;
        } else {
            col_count++;
        }
        row_size += *col;
    }

    row_size += 2 * (col_count - 1) + 1;
    sb_grow(out, row_size * (data->len + 1));

    sb_write_table(out, hdr, data, col_sizes, 0);
}

void sb_add_csv_table(sb_t *out, const qv_t(table_hdr) *hdr,
                      const qv_t(table_data) *data, int sep)
{
    int *populated_cols = p_alloca(int, hdr->len);

    /* Check if we have empty columns. */
    tab_for_each_pos(pos, hdr) {
        table_hdr_t *col_hdr = &hdr->tab[pos];
        int *populated_col = &populated_cols[pos];

        *populated_col = false;

        if (!col_hdr->omit_if_empty || col_hdr->empty_value.len) {
            /* If table_hdr__t has a default empty value, the column will not
             * be omitted.
             */
            *populated_col = true;
            continue;
        }

        tab_for_each_ptr(row, data) {
            if (row->len <= pos) {
                /* Not enough data columns. */
                break;
            }

            if (row->tab[pos].len) {
                /* Column will not be omitted. */
                *populated_col = true;
                break;
            }
        }
    }

    sb_write_table(out, hdr, data, populated_cols, sep);
}

/* }}} */
/* {{{ Tests */

#include <lib-common/z.h>

Z_GROUP_EXPORT(str_buf_pp) {
    Z_TEST(add_table, "sb_add_table") {
        t_scope;
        t_SB_1k(sb);
        qv_t(table_hdr) hdr;
        qv_t(lstr) *row;
        qv_t(table_data) data;
        table_hdr_t hdr_data[] = { {
                .title = LSTR_IMMED("COL A"),
            }, {
                .title = LSTR_IMMED("COL B"),
            }, {
                .title = LSTR_IMMED("COL C"),
            }
        };

        qv_init_static(&hdr, hdr_data, countof(hdr_data));
        t_qv_init(&data, 2);
        row = qv_growlen(&data, 1);
        t_qv_init(row, countof(hdr_data));
        qv_append(row, LSTR("col A - rôw 1"));
        qv_append(row, LSTR("col B - row 1"));
        row = qv_growlen(&data, 1);
        t_qv_init(row, countof(hdr_data));
        qv_append(row, LSTR("col A - row 2"));
        qv_append(row, LSTR("çôl B - row 2"));

        sb_reset(&sb);
        sb_add_table(&sb, &hdr, &data);
        Z_ASSERT_STREQUAL(sb.data, "COL A          COL B          COL C\n"
                                   "col A - rôw 1  col B - row 1  \n"
                                   "col A - row 2  çôl B - row 2  \n");

        sb_reset(&sb);
        sb_add_csv_table(&sb, &hdr, &data, ';');
        Z_ASSERT_STREQUAL(sb.data, "COL A;COL B;COL C\n"
                                   "col A - rôw 1;col B - row 1;\n"
                                   "col A - row 2;çôl B - row 2;\n");

        hdr_data[0].max_width = 7;
        hdr_data[1].min_width = 20;
        hdr_data[2].omit_if_empty = true;

        sb_reset(&sb);
        sb_add_table(&sb, &hdr, &data);
        Z_ASSERT_STREQUAL(sb.data, "COL A    COL B               \n"
                                   "col A -  col B - row 1       \n"
                                   "col A -  çôl B - row 2       \n");

        sb_reset(&sb);
        sb_add_csv_table(&sb, &hdr, &data, ';');
        Z_ASSERT_STREQUAL(sb.data, "COL A;COL B\n"
                                   "col A - rôw 1;col B - row 1\n"
                                   "col A - row 2;çôl B - row 2\n");

        hdr_data[0].max_width = 7;
        hdr_data[0].add_ellipsis = true;
        hdr_data[1].min_width = 0;
        hdr_data[2].omit_if_empty = false;
        hdr_data[2].empty_value = LSTR("-");

        sb_reset(&sb);
        sb_add_table(&sb, &hdr, &data);
        Z_ASSERT_STREQUAL(sb.data, "COL A    COL B          COL C\n"
                                   "col A …  col B - row 1  -\n"
                                   "col A …  çôl B - row 2  -\n");

        sb_reset(&sb);
        sb_add_csv_table(&sb, &hdr, &data, ';');
        Z_ASSERT_STREQUAL(sb.data, "COL A;COL B;COL C\n"
                                   "col A - rôw 1;col B - row 1;-\n"
                                   "col A - row 2;çôl B - row 2;-\n");

        hdr_data[2].align = ALIGN_RIGHT;

        sb_reset(&sb);
        sb_add_table(&sb, &hdr, &data);
        Z_ASSERT_STREQUAL(sb.data, "COL A    COL B          COL C\n"
                                   "col A …  col B - row 1      -\n"
                                   "col A …  çôl B - row 2      -\n");

        hdr_data[2].align = ALIGN_CENTER;

        sb_reset(&sb);
        sb_add_table(&sb, &hdr, &data);
        Z_ASSERT_STREQUAL(sb.data, "COL A    COL B          COL C\n"
                                   "col A …  col B - row 1    -\n"
                                   "col A …  çôl B - row 2    -\n");

        /* Add a row with characters that will be escaped. */
        row = qv_growlen(&data, 1);
        t_qv_init(row, countof(hdr_data));
        qv_append(row, LSTR("col A -\n \"row\" 3"));
        qv_append(row, LSTR("çôl B -\r row 3"));

        sb_reset(&sb);
        sb_add_csv_table(&sb, &hdr, &data, ';');
        Z_ASSERT_STREQUAL(sb.data,
            "COL A;COL B;COL C\n"
            "col A - rôw 1;col B - row 1;-\n"
            "col A - row 2;çôl B - row 2;-\n"
            "\"col A -\n \"\"row\"\" 3\";\"çôl B -\r row 3\";-\n");

        qv_clear(&data);
        row = qv_growlen(&data, 1);
        t_qv_init(row, countof(hdr_data));
        qv_append(row, LSTR_NULL_V);
        qv_append(row, LSTR("col B - row 1"));
        hdr_data[0].omit_if_empty = true;

        sb_reset(&sb);
        sb_add_table(&sb, &hdr, &data);
        Z_ASSERT_STREQUAL(sb.data, "COL B          COL C\n"
                                   "col B - row 1    -\n");

        /* Header with empty value. */
        hdr_data[2].title = LSTR_EMPTY_V;
        hdr_data[2].empty_value = LSTR_EMPTY_V;
        qv_clear(&data);
        row = qv_growlen(&data, 1);
        t_qv_init(row, countof(hdr_data));
        qv_append(row, LSTR("col A"));
        qv_append(row, LSTR("col B"));
        qv_append(row, LSTR("col C"));

        sb_reset(&sb);
        sb_add_table(&sb, &hdr, &data);
        Z_ASSERT_STREQUAL(sb.data, "COL A  COL B  \n"
                                   "col A  col B  col C\n");


    } Z_TEST_END;
} Z_GROUP_END

/* }}} */
