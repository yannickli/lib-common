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
#include "blob.h"

typedef struct xml_prop_t xml_prop_t;
typedef struct xml_tag_t xml_tag_t;
typedef struct xml_tree_t xml_tree_t;

struct xml_tag_t {
    xml_tag_t *next;
    xml_tag_t *parent;
    char *fullname;
    /* name points to the name without the namespace, inside fullname */
    const char *name;
    int name_hash;
    xml_prop_t *property;
    xml_tag_t *child;
    char *text;
};

struct xml_prop_t {
    xml_prop_t *next;
    char *name;
    int name_hash;
    char *value;
};

struct xml_tree_t {
    xml_tag_t *root;
    /* Memory pool for text: names, properties, values... They are
     * all allocated at parse time and freed at the same time, so we 
     * pre-allocate them in one big chunk, bigger than what's really
     * needed but we do not really care loosing a few bytes. */
    char *mp_start;
    char *mp_cur;
    int mp_left;
    // Add version, charset, etc.
};

xml_tree_t *xml_new_tree(const char *payload, size_t len, char *error_buf,
                         size_t buflen);
const xml_tag_t *xml_search(const xml_tree_t *tree,
                            const xml_tag_t *previous,
                            const char *pattern);
/*TODO do search on tag */
void xml_tree_delete(xml_tree_t **tree);
void xml_tag_delete(xml_tag_t **t);
void blob_append_tree(const xml_tree_t *tree, blob_t *blob);
void blob_append_branch(const xml_tag_t *root, blob_t *blob,
                        const char * prefix);

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
#include <check.h>

Suite *check_xml_suite(void);

#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
#endif /* IS_LIB_COMMON_XML_H */
