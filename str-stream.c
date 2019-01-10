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

#include "net.h"
#include "str.h"
#include "container.h"

int ps_copyv(pstream_t *ps, struct iovec *iov, size_t *iov_len, int *flags)
{
    int orig_len = ps_len(ps);
    size_t i;

    for (i = 0; !ps_done(ps) && i < *iov_len; i++) {
        if (iov[i].iov_len > ps_len(ps))
            iov[i].iov_len = ps_len(ps);
        memcpy(iov[i].iov_base, ps->b, iov[i].iov_len);
        ps_skip(ps, iov[i].iov_len);
    }
    *iov_len = i;

    if (flags) {
        if (ps_done(ps)) {
            *flags &= ~MSG_TRUNC;
            return orig_len;
        }
        if (*flags & MSG_TRUNC)
            return orig_len;
        *flags |= MSG_TRUNC;
    }
    return orig_len - ps_len(ps);
}

static int ps_get_csv_quoted_field(mem_pool_t *mp, pstream_t *ps, int quote,
                                   qv_t(lstr) *fields)
{
    SB_8k(sb);

    __ps_skip(ps, 1);

    for (;;) {
        pstream_t part;

        PS_CHECK(ps_get_ps_chr_and_skip(ps, quote, &part));
        if (!ps_done(ps) && *ps->s == quote) {
            __ps_skip(ps, 1);
            sb_add(&sb, part.s, ps_len(&part) + 1);
        } else
        if (sb.len == 0) {
            qv_append(lstr, fields, LSTR_PS_V(&part));
            return 0;
        } else {
            sb_add(&sb, part.s, ps_len(&part));

            if (mp) {
                qv_append(lstr, fields, mp_lstr_dups(mp, sb.data, sb.len));
            } else {
                lstr_t dst;

                lstr_transfer_sb(&dst, &sb, false);
                qv_append(lstr, fields, dst);
            }
            return 0;
        }
    }

    return 0;
}

int ps_get_csv_line(mem_pool_t *mp, pstream_t *ps, int sep, int quote,
                    qv_t(lstr) *fields)
{
    ctype_desc_t cdesc;
    char cdesc_tok[] = { '\r', '\n', sep, '\0' };

    ctype_desc_build(&cdesc, cdesc_tok);

    if (ps_done(ps)) {
        return 0;
    }

    for (;;) {
        if (ps_done(ps)) {
            qv_append(lstr, fields, LSTR_NULL_V);
            return 0;
        } else
        if (*ps->s == quote) {
            PS_CHECK(ps_get_csv_quoted_field(mp, ps, quote, fields));
        } else {
            pstream_t field = ps_get_cspan(ps, &cdesc);

            if (!ps_len(&field)) {
                qv_append(lstr, fields, LSTR_NULL_V);
            } else {
                qv_append(lstr, fields, LSTR_PS_V(&field));
            }
        }

        switch (ps_getc(ps)) {
          case '\r':
            return ps_skipc(ps, '\n');

          case '\n':
          case -1:
            return 0;

          default:
            PS_WANT(ps->s[-1] == sep);
            break;
        }
    }

    return 0;
}

void ps_split(pstream_t ps, const ctype_desc_t *sep, unsigned flags,
              qv_t(lstr) *res)
{
    if (flags & PS_SPLIT_SKIP_EMPTY) {
        ps_skip_span(&ps, sep);
    }
    while (!ps_done(&ps)) {
        pstream_t n = ps_get_cspan(&ps, sep);

        qv_append(lstr, res, LSTR_PS_V(&n));
        if (flags & PS_SPLIT_SKIP_EMPTY) {
            ps_skip_span(&ps, sep);
        } else {
            ps_skip(&ps, 1);
        }
    }
}
