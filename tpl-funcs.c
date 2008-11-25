/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "str.h"
#include "tpl.h"


int tpl_compute_len_copy(blob_t *b, tpl_t **args, int nb, int len)
{
    if (b) {
        while (--nb >= 0) {
            tpl_t *in = *args++;

            if (in->op == TPL_OP_BLOB) {
                blob_append(b, &in->u.blob);
                len += in->u.blob.len;
            } else {
                assert (in->op == TPL_OP_DATA);
                blob_append_data(b, in->u.data.data, in->u.data.len);
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

int tpl_encode_plmn(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
{
    const byte *data;
    int len;
    int64_t num;

    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    if (tpl_get_short_data(args, nb, &data, &len))
        return -1;

    num = msisdn_canonify((const char*)data, len, -1);
    if (num < 0) {
        blob_append_data(blob, data, len);
    } else {
        blob_append_fmt(blob, "+%lld/TYPE=PLMN", (long long)num);
    }
    return 0;
}

int tpl_encode_msisdn_canonify(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
{
    const byte *data;
    int len;
    int64_t num;

    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    if (tpl_get_short_data(args, nb, &data, &len))
        return -1;

    num = msisdn_canonify((const char*)data, len, -1);
    if (num < 0) {
        blob_append_data(blob, data, len);
    } else {
        blob_append_fmt(blob, "%lld", (long long)num);
    }
    return 0;
}

/****************************************************************************/
/* Escapings                                                                */
/****************************************************************************/

int tpl_encode_xml(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
{
    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *in = *args++;
        if (in->op == TPL_OP_DATA) {
            blob_append_xml_escape(blob, in->u.data.data, in->u.data.len);
        } else {
            assert (in->op == TPL_OP_BLOB);
            blob_append_xml_escape(blob, in->u.blob.data, in->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_url(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
{
    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *in = *args++;
        if (in->op == TPL_OP_DATA) {
            blob_append_urlencode(blob, in->u.data.data, in->u.data.len);
        } else {
            assert (in->op == TPL_OP_BLOB);
            blob_append_urlencode(blob, in->u.blob.data, in->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_latin1(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
{
    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *in = *args++;
        if (in->op == TPL_OP_DATA) {
            blob_utf8_to_latin1_n(blob, (const char *)in->u.data.data,
                                  in->u.data.len, '.');
        } else {
            assert (in->op == TPL_OP_BLOB);
            blob_utf8_to_latin1_n(blob, (const char *)in->u.blob.data,
                                  in->u.blob.len, '.');
        }
    }
    return 0;
}

int tpl_encode_ira(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
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

int tpl_encode_ira_bin(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
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

int tpl_encode_base64(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
{
    base64enc_ctx ctx;

    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    blob_append_base64_start(blob, 0, 0, &ctx);

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            blob_append_base64_update(blob, arg->u.data.data, arg->u.data.len,
                                      &ctx);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            blob_append_base64_update(blob, arg->u.blob.data, arg->u.blob.len,
                                      &ctx);
        }
    }
    blob_append_base64_finish(blob, &ctx);

    return 0;
}

int tpl_encode_qp(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
{
    if (!blob) {
        assert(out);
        blob = tpl_get_blob(out);
    }

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            blob_append_quoted_printable(blob, arg->u.data.data,
                                         arg->u.data.len);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            blob_append_quoted_printable(blob, arg->u.blob.data,
                                         arg->u.blob.len);
        }
    }
    return 0;
}

int tpl_encode_wbxml_href(tpl_t *out, blob_t *blob, tpl_t **args, int nb)
{
    tpl_t *arg0;
    blob_t tmp;

    assert (nb > 0);
    arg0 = *args;

    if (!blob) {
        assert (out);
        blob = tpl_get_blob(out);
    }

    blob_inita(&tmp, 1024);

    while (--nb >= 0) {
        tpl_t *arg = *args++;
        if (arg->op == TPL_OP_DATA) {
            blob_append_data(&tmp, arg->u.data.data, arg->u.data.len);
        } else {
            assert (arg->op == TPL_OP_BLOB);
            blob_append(&tmp, &arg->u.blob);
        }
    }

    blob_append_wbxml_href(blob, tmp.data, tmp.len);
    blob_wipe(&tmp);

    return 0;
}
