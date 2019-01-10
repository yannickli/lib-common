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

#ifndef __clang__
__attribute__((flatten))
#endif
int F(xmlr_get)(xml_reader_t xr, ARGS_P)
{
    assert (xmlr_on_element(xr, false));

    if (xmlTextReaderIsEmptyElement(xr) == 1) {
        RETHROW(F(xmlr_val)(xr, NULL, ARGS));
        return xmlr_next_node(xr);
    }

    if (RETHROW(xmlTextReaderRead(xr)) != 1)
        return xmlr_fail(xr, "expecting text or closing element");
    RETHROW(xmlr_scan_node(xr, true));
    if (xmlTextReaderNodeType(xr) == XTYPE(TEXT)) {
        const char *s = (const char *)xmlTextReaderConstValue(xr);

        RETHROW(F(xmlr_val)(xr, s, ARGS));
        RETHROW(xmlr_scan_node(xr, false));
    } else {
        RETHROW(F(xmlr_val)(xr, NULL, ARGS));
    }
    if (xmlTextReaderNodeType(xr) != XTYPE(END_ELEMENT))
        return xmlr_fail(xr, "expecting closing tag");

    return xmlr_next_node(xr);
}

#undef F
#undef ARGS
#undef ARGS_P
