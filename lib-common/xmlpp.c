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

#include "xmlpp.h"

void xmlpp_open(xmlpp_t *pp, blob_t *buf)
{
    p_clear(pp, 1);
    pp->buf = buf;
    string_array_init(&pp->stack);
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

void xmlpp_opentag(xmlpp_t *pp, const char *tag)
{
    blob_append_fmt(pp->buf, "\n%*c<%s>",
                    (int)pp->stack.len * 2, ' ', tag);
    string_array_append(&pp->stack, p_strdup(tag));
    pp->can_do_attr = true;
}

void xmlpp_putattr(xmlpp_t *pp, const char *key, const char *val)
{
    if (!pp->can_do_attr)
        return;

    blob_kill_last(pp->buf, 1);
    blob_append_fmt(pp->buf, " %s=\"", key);
    blob_append_xmlescaped(pp->buf, val, -1);
    blob_append_cstr(pp->buf, "\">");
}

void xmlpp_puttext(xmlpp_t *pp, const char *s, int len)
{
    pp->can_do_attr = false;
    blob_append_xmlescaped(pp->buf, s, len);
}

void xmlpp_closetag(xmlpp_t *pp, int noindent)
{
    char *tag;
    if (!pp->stack.len)
        return;

    tag = string_array_take(&pp->stack, pp->stack.len - 1);
    if (pp->can_do_attr) {
        blob_kill_last(pp->buf, 1);
        blob_append_cstr(pp->buf, " />");
    } else
    if (!noindent) {
        blob_append_fmt(pp->buf, "\n%*c</%s>",
                        (int)pp->stack.len * 2, ' ', tag);
    } else {
        blob_append_fmt(pp->buf, "</%s>", tag);
    }
    pp->can_do_attr = false;
    p_delete(&tag);
}

void xmlpp_close(xmlpp_t *pp)
{
    while (pp->stack.len)
        xmlpp_closetag(pp, false);
    if (pp->buf->data[pp->buf->len - 1] != '\n')
        blob_append_byte(pp->buf, '\n');
    string_array_wipe(&pp->stack);
}
