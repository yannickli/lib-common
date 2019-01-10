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

__flatten
int F(xmlr_getattr)(xml_reader_t xr, xmlAttrPtr attr, ARGS_P)
{
    const char *name = (const char *)attr->name;
    xmlNodePtr  n    = attr->children;

    assert (xmlr_on_element(xr, false));

    if (n == NULL)
        return F(xmlr_attr)(xr, name, NULL, ARGS);
    if (n->next == NULL
    &&  (n->type == XML_TEXT_NODE || n->type == XML_CDATA_SECTION_NODE))
    {
        return F(xmlr_attr)(xr, name, (const char *)n->content, ARGS);
    } else {
        char *s = (char *)xmlNodeListGetString(attr->doc, n, 1);
        int res = F(xmlr_attr)(xr, name, s, ARGS);

        xmlFree(s);
        return res;
    }
}

#undef F
#undef ARGS
#undef ARGS_P
