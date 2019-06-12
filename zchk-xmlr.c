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

/* LCOV_EXCL_START */

#include "xmlr.h"
#include "z.h"

Z_GROUP_EXPORT(xmlr)
{
    Z_TEST(xmlr_node_get_xmlns, "xmlr_node_get_xmlns") {
        lstr_t body = LSTR("<ns:elt xmlns:ns=\"ns_uri\" />");
        lstr_t name;
        lstr_t ns;

        Z_ASSERT(xmlr_setup(&xmlr_g, body.s, body.len) >= 0);

        Z_ASSERT(xmlr_node_get_local_name(xmlr_g, &name) >= 0);
        Z_ASSERT_STREQUAL(name.s, "elt");

        ns = xmlr_node_get_xmlns(xmlr_g);
        Z_ASSERT_STREQUAL(ns.s, "ns");

        xmlr_close(&xmlr_g);
    } Z_TEST_END;

    Z_TEST(xmlr_node_get_xmlns_uri, "xmlr_node_get_xmlns_uri") {
        lstr_t body = LSTR("<ns:elt xmlns:ns=\"ns_uri\" />");
        lstr_t name;
        lstr_t ns_uri;

        Z_ASSERT(xmlr_setup(&xmlr_g, body.s, body.len) >= 0);

        Z_ASSERT(xmlr_node_get_local_name(xmlr_g, &name) >= 0);
        Z_ASSERT_STREQUAL(name.s, "elt");

        ns_uri = xmlr_node_get_xmlns_uri(xmlr_g);
        Z_ASSERT_STREQUAL(ns_uri.s, "ns_uri");

        xmlr_close(&xmlr_g);
    } Z_TEST_END;

    Z_TEST(xmlr_node_get_xmlns_no_uri, "xmlr_node_get_xmlns_no_uri") {
        lstr_t body = LSTR("<elt xmlns:ns=\"ns_uri\" />");
        lstr_t name;
        lstr_t ns_uri;

        Z_ASSERT(xmlr_setup(&xmlr_g, body.s, body.len) >= 0);

        Z_ASSERT(xmlr_node_get_local_name(xmlr_g, &name) >= 0);
        Z_ASSERT_STREQUAL(name.s, "elt");

        ns_uri = xmlr_node_get_xmlns_uri(xmlr_g);
        Z_ASSERT(ns_uri.len == 0);

        xmlr_close(&xmlr_g);
    } Z_TEST_END;

    Z_TEST(node_should_close, "") {
        t_scope;
        lstr_t xml = LSTR("<root>                                       "
                          "    <child1>                                 "
                          "       <granchild>value_granchild</granchild>"
                          "    </child1>                                "
                          "    <child2>value_child2</child2>            "
                          "    <child3 attr=\"autoclosing\" />          "
                          "    <child4><!--empty-->  </child4>          "
                          " </root>");
        lstr_t val = LSTR_NULL;

        xmlr_setup(&xmlr_g, xml.s, xml.len);
        Z_ASSERT_EQ(xmlr_node_open_s(xmlr_g, "root"), 1);
        Z_ASSERT_EQ(xmlr_node_open_s(xmlr_g, "child1"), 1);
        Z_ASSERT_ZERO(t_xmlr_get_str(xmlr_g, false, &val));
        Z_ASSERT_LSTREQUAL(val, LSTR("value_granchild"));
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g)); /* </child1> */
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child2"), 1);
        Z_ASSERT_ZERO(xmlr_node_is_closing(xmlr_g)); /* not empty */
        Z_ASSERT_ZERO(t_xmlr_get_str(xmlr_g, false, &val));
        Z_ASSERT_LSTREQUAL(val, LSTR("value_child2"));
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child3"), 1);
        /* </child3> autoclosing, should close */
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g));
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child4"), 1);
        /* </child4> empty, should close */
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g));
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g)); /* </root> should close */
    } Z_TEST_END;

    Z_TEST(node_should_not_close, "") {
        lstr_t xml = LSTR("<root>"
                          "    <child>value_child</child>"
                          "</root>");

        xmlr_setup(&xmlr_g, xml.s, xml.len);
        Z_ASSERT_EQ(xmlr_node_open_s(xmlr_g, "root"), 1);
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child"), 1);
        Z_ASSERT_NEG(xmlr_node_close(xmlr_g));
    } Z_TEST_END;

    Z_TEST(node_should_close_2, "") {
        lstr_t xml = LSTR("<root>"
                          "    <child>value_child</child>"
                          "</root>");

        xmlr_setup(&xmlr_g, xml.s, xml.len);
        Z_ASSERT_EQ(xmlr_node_open_s(xmlr_g, "root"), 1);
        Z_ASSERT_EQ(xmlr_node_is_s(xmlr_g, "child"), 1);
        Z_ASSERT_ZERO(xmlr_node_skip_s(xmlr_g, "child", XMLR_ENTER_EMPTY_OK));
        Z_ASSERT_ZERO(xmlr_node_close(xmlr_g));
    } Z_TEST_END;
} Z_GROUP_END;

/* LCOV_EXCL_STOP */
