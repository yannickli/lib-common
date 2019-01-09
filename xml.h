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

#ifndef IS_LIB_COMMON_XML_H
#define IS_LIB_COMMON_XML_H

#include "core.h"

typedef struct xml_prop_t xml_prop_t;
typedef struct xml_tag_t xml_tag_t;
typedef struct xml_tree_t xml_tree_t;

struct xml_tag_t {
    xml_tag_t *next;
    xml_tag_t *parent;
    char *fullname;
    /* name points to the name without the namespace, inside fullname */
    const char *name;
    uint32_t name_hash;
    xml_prop_t *property;
    xml_tag_t *child;
    char *text;
};

struct xml_prop_t {
    xml_prop_t *next;
    char *name;
    uint32_t name_hash;
    char *value;
};

struct xml_tree_t {
    /* Memory pool for everything in the tree: tags, attributes, texts, ... */
    mem_pool_t *mp;

    xml_tag_t *root;
    // Add version, charset, etc.
};

xml_tree_t *xml_new_tree(const char *payload, size_t len, char *error_buf,
                         size_t buflen);
const xml_tag_t *xml_search_branch(const xml_tag_t *branch,
                                   const xml_tag_t **previous,
                                   const char *pattern);

const xml_tag_t *xml_search_subtree(const xml_tree_t *tree,
                                    const xml_tag_t *subtree,
                                    const xml_tag_t *previous,
                                    const char *pattern);
static inline
const xml_tag_t *xml_search(const xml_tree_t *tree,
                            const xml_tag_t *previous,
                            const char *pattern)
{
    return xml_search_subtree(tree, tree->root, previous, pattern);
}

void xml_tree_wipe(xml_tree_t *tree);
GENERIC_DELETE(xml_tree_t, xml_tree);

void sb_add_xmltag(sb_t *sb, const xml_tag_t *root);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_xml_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_XML_H */
