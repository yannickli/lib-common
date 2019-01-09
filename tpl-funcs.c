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

#include "tpl.h"
#include "blob.h"

int tpl_compute_len_copy(sb_t *b, tpl_t **args, int nb, int len)
{
    if (b) {
        while (--nb >= 0) {
            tpl_t *in = *args++;

            if (in->op == TPL_OP_BLOB) {
                sb_addsb(b, &in->u.blob);
                len += in->u.blob.len;
            } else {
                assert (in->op == TPL_OP_DATA);
                sb_add(b, in->u.data.data, in->u.data.len);
                len += in->u.data.len;
            }
        }
    } else {
        while (--nb >= 0) {
            tpl_t *in = *args++;

            if (in->op == TPL_OP_BLOB) {
                len += in->u.blob.len;
            } else {
                assert (in->op == TPL_OP_DATA);
                len += in->u.data.len;
            }
        }
    }
    return len;
}

/****************************************************************************/
/* Short formats                                                            */
/****************************************************************************/


/****************************************************************************/
/* Escapings                                                                */
/****************************************************************************/

int tpl_encode_xml(tpl_t *out, sb_t *blob, tpl_t **args, int nb)
{
    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *in = *args++;
        if (in->op == TPL_OP_DATA) {
            sb_add_xmlescape(blob, in->u.data.data, in->u.data.len);
        } else {
            assert (in->op == TPL_OP_BLOB);
            sb_add_xmlescape(blob, in->u.blob.data, in->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_url(tpl_t *out, sb_t *blob, tpl_t **args, int nb)
{
    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *in = *args++;
        if (in->op == TPL_OP_DATA) {
            sb_add_urlencode(blob, in->u.data.data, in->u.data.len);
        } else {
            assert (in->op == TPL_OP_BLOB);
            sb_add_urlencode(blob, in->u.blob.data, in->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_latin1(tpl_t *out, sb_t *blob, tpl_t **args, int nb)
{
    int res = 0;

    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *in = *args++;
        if (in->op == TPL_OP_DATA) {
            res |= sb_conv_to_latin1(blob, in->u.data.data, in->u.data.len, '.');
        } else {
            assert (in->op == TPL_OP_BLOB);
            res |= sb_conv_to_latin1(blob, in->u.blob.data, in->u.blob.len, '.');
        }
    }
    return res;
}

int tpl_encode_ira(tpl_t *out, sb_t *blob, tpl_t **args, int nb)
{
    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            blob_append_ira_hex(blob, arg->u.data.data, arg->u.data.len);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            blob_append_ira_hex(blob, arg->u.blob.data, arg->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_ira_bin(tpl_t *out, sb_t *blob, tpl_t **args, int nb)
{
    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            blob_append_ira_bin(blob, arg->u.data.data, arg->u.data.len);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            blob_append_ira_bin(blob, arg->u.blob.data, arg->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_ucs2be(tpl_t *out, sb_t *sb, tpl_t **args, int nb)
{
    if (!sb) {
        assert(out);
        sb = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            sb_conv_to_ucs2be(sb, arg->u.data.data, arg->u.data.len);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            sb_conv_to_ucs2be(sb, arg->u.blob.data, arg->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_ucs2be_hex(tpl_t *out, sb_t *sb, tpl_t **args, int nb)
{
    if (!sb) {
        assert(out);
        sb = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            sb_conv_to_ucs2be_hex(sb, arg->u.data.data, arg->u.data.len);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            sb_conv_to_ucs2be_hex(sb, arg->u.blob.data, arg->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_base64(tpl_t *out, sb_t *blob, tpl_t **args, int nb)
{
    sb_b64_ctx_t ctx;

    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    sb_add_b64_start(blob, 0, 0, &ctx);

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            sb_add_b64_update(blob, arg->u.data.data, arg->u.data.len, &ctx);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            sb_add_b64_update(blob, arg->u.blob.data, arg->u.blob.len, &ctx);
        }
    }
    sb_add_b64_finish(blob, &ctx);

    return 0;
}

int tpl_encode_qp(tpl_t *out, sb_t *blob, tpl_t **args, int nb)
{
    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            sb_add_qpe(blob, arg->u.data.data, arg->u.data.len);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            sb_add_qpe(blob, arg->u.blob.data, arg->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_wbxml_href(tpl_t *out, sb_t *blob, tpl_t **args, int nb)
{
    SB_1k(tmp);

    assert (nb > 0);

    if (!blob) {
        assert (out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            sb_add(&tmp, arg->u.data.data, arg->u.data.len);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            sb_addsb(&tmp, &arg->u.blob);
        }
    }

    blob_append_wbxml_href(blob, (const byte *)tmp.data, tmp.len);
    sb_wipe(&tmp);

    return 0;
}
