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
