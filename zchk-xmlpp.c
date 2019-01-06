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

#include "xmlpp.h"

#include "z.h"

Z_GROUP_EXPORT(xmlpp) {
    Z_TEST(xmlpp_tag_scope, "xmlpp_tag_scope") {
        xmlpp_t pp;
        SB_8k(xml1);
        SB_8k(xml2);

        xmlpp_open_banner(&pp, &xml1);
        xmlpp_opentag(&pp, "level1");
        xmlpp_opentag(&pp, "level2");
        xmlpp_putattr(&pp, "attr", "foo");
        xmlpp_closetag(&pp);
        xmlpp_closetag(&pp);
        xmlpp_close(&pp);

        xmlpp_open_banner(&pp, &xml2);
        xmlpp_tag_scope(&pp, "level1") {
            xmlpp_tag_scope(&pp, "level2") {
                xmlpp_putattr(&pp, "attr", "foo");
            }
        }
        xmlpp_close(&pp);

        Z_ASSERT_STREQUAL(xml1.data, xml2.data,
                          "xml created with xmlpp_opentag/xmlpp_closetag "
                          "or xmlpp_tag_scope should be the same");
    } Z_TEST_END;
} Z_GROUP_END;
