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

#include "xmlbuf.h"

xmlbuf_t *xmlbuf_init(xmlbuf_t *buf)
{
    p_clear(buf, 1);
    blob_init(&buf->buf);
    blob_set_cstr(&buf->buf, "<?xml version=\"1.0\"?>\n");
    string_array_init(&buf->stack);
    return buf;
}

void xmlbuf_wipe(xmlbuf_t *buf)
{
    blob_wipe(&buf->buf);
    string_array_wipe(&buf->stack);
}

static void blob_append_xmlescaped(blob_t *buf, const char *s, int len)
{
    if (len < 0)
        len = strlen(s);
    for (;;) {
        int pos = 0;

        while (pos < len && !strchr("&'\"<>", s[pos]))
            pos++;
        blob_append_data(buf, s, pos);
        if (pos == len)
            return;

        switch (s[pos]) {
#define CASE(c, s)  case c: blob_append_cstr(buf, s); break
            CASE('&', "&amp;");
            CASE('\'', "&apos;");
            CASE('"', "&quot;");
            CASE('<', "&lt;");
          default:
            CASE('>', "&gt;");
#undef CASE
        }
        s += pos + 1;
        len -= pos + 1;
    }
}

void xmlbuf_opentag(xmlbuf_t *buf, const char *tag)
{
    blob_append_fmt(&buf->buf, "\n%*c<%s>",
                    (int)buf->stack.len * 2, ' ', tag);
    string_array_append(&buf->stack, p_strdup(tag));
    buf->can_do_attr = true;
}

void xmlbuf_putattr(xmlbuf_t *buf, const char *key, const char *val)
{
    if (!buf->can_do_attr)
        return;

    blob_kill_last(&buf->buf, 1);
    blob_append_fmt(&buf->buf, " %s=\"", key);
    blob_append_xmlescaped(&buf->buf, val, -1);
    blob_append_cstr(&buf->buf, "\">");
}

void xmlbuf_puttext(xmlbuf_t *buf, const char *s, int len)
{
    buf->can_do_attr = false;
    blob_append_xmlescaped(&buf->buf, s, len);
}

void xmlbuf_closetag(xmlbuf_t *buf, int noindent)
{
    char *tag;
    if (!buf->stack.len)
        return;

    tag = string_array_take(&buf->stack, buf->stack.len - 1);
    if (buf->can_do_attr) {
        blob_kill_last(&buf->buf, 1);
        blob_append_cstr(&buf->buf, " />");
    } else
    if (!noindent) {
        blob_append_fmt(&buf->buf, "\n%*c</%s>",
                        (int)buf->stack.len * 2, ' ', tag);
    } else {
        blob_append_fmt(&buf->buf, "</%s>", tag);
    }
    buf->can_do_attr = false;
    p_delete(&tag);
}

void xmlbuf_close(xmlbuf_t *buf)
{
    while (buf->stack.len)
        xmlbuf_closetag(buf, false);
    if (buf->buf.data[buf->buf.len - 1] != '\n')
        blob_append_byte(&buf->buf, '\n');
}
