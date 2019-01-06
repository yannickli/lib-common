/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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

    if (len <= 0) {
        content = col->empty_value;
        len = lstr_utf8_strlen(content);
    }

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

    /* Write the header */
    tab_for_each_pos(pos, hdr) {
        if (col_sizes[pos] == 0) {
            continue;
        }
        if (pos != 0) {
            sb_adds(out, "  ");
        }
        sb_add_cell(out, &hdr->tab[pos], col_sizes[pos], true,
                        pos == hdr->len - 1, hdr->tab[pos].title);
    }
    sb_addc(out, '\n');

    /* Write the content */
    tab_for_each_ptr(row, data) {
        tab_for_each_pos(pos, hdr) {
            lstr_t content = LSTR_NULL;

            if (col_sizes[pos] == 0) {
                continue;
            }
            if (pos < row->len) {
                content = row->tab[pos];
            }

            if (pos != 0) {
                sb_adds(out, "  ");
            }
            sb_add_cell(out, &hdr->tab[pos], col_sizes[pos], false,
                            pos == hdr->len - 1, content);
        }
        sb_addc(out, '\n');
    }
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

        hdr_data[0].max_width = 7;
        hdr_data[1].min_width = 20;
        hdr_data[2].omit_if_empty = true;

        sb_reset(&sb);
        sb_add_table(&sb, &hdr, &data);
        Z_ASSERT_STREQUAL(sb.data, "COL A    COL B               \n"
                                   "col A -  col B - row 1       \n"
                                   "col A -  çôl B - row 2       \n");

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
    } Z_TEST_END;
} Z_GROUP_END

/* }}} */
