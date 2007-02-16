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

#ifndef IS_LIB_COMMON_XML_H
#define IS_LIB_COMMON_XML_H

#include "macros.h"

typedef struct xml_prop_t xml_prop_t;
typedef struct xml_tag_t xml_tag_t;
typedef struct xml_tree_t xml_tree_t;

struct xml_tag_t {
    char *name;
    xml_prop_t *property;
    xml_tag_t *child;
    xml_tag_t *next;
    char *text;
};

struct xml_prop_t {
    char *name;
    char *value;
    xml_prop_t *next;
};

struct xml_tree_t {
    xml_tag_t *root;
    // Add version, charset, etc.
};

xml_tree_t *xml_new_tree(const char *payload, size_t len);
void xml_delete_tree(xml_tree_t **tree);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_xml_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_XML_H */
