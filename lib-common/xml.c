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

#include <ctype.h>
#include <string.h>
#include <lib-common/mem.h>
#include "strconv.h"
#include "xml.h"
#include "list.h"

static void xml_prop_t_delete(xml_prop_t **p)
{
    if (!p || !*p) {
        return;
    }
    p_delete(&(*p)->name);
    p_delete(&(*p)->value);
    (*p)->next = NULL;
    p_delete(p);
}

SLIST_FUNCTIONS(xml_prop_t, xml_prop_t)

SLIST_PROTOS(xml_tag_t, xml_tag_t)
static void xml_tag_t_delete(xml_tag_t **t)
{
    if (t && *t) {
        p_delete(&(*t)->fullname);
        xml_tag_t_list_wipe(&(*t)->child);
        xml_prop_t_list_wipe(&(*t)->property);
        p_delete(&(*t)->text);
    }
    p_delete(t);
}

SLIST_FUNCTIONS(xml_tag_t, xml_tag_t)

typedef enum parse_t {
    PARSE_EOF,
    PARSE_END_TAG,
    PARSE_TAG,
    PARSE_PROP,
    PARSE_END_PROP,
    PARSE_ERROR,
} parse_t;

#define SKIPSPACES(p, len)      \
    while (len) {               \
        if (!isspace(*p)) {     \
            break;              \
        }                       \
        len--;                  \
        p++;                    \
    }
#define ENSURE(p, len, c)           \
    do {                            \
        if (len <= 0 || *p != c) {  \
            goto error;             \
        }                           \
    } while (0)


/* Parse a property inside a tag and put it in (*dst).
 * Property is of form : prop = "value"
 * Single/Double quoting is supported.
 */
static parse_t xml_get_prop(xml_prop_t **dst, const char *payload,
                            size_t len, const char **pend)
{
    const char *name, *value;
    const char *p;
    xml_prop_t *prop;
    char quot;
    int unquoted_len;
    parse_t ret;

    prop = NULL;
    p = payload;

    SKIPSPACES(p, len);

    if (len <= 0) {
        goto end;
    }
    if (*p == '>') {
        goto end;
    }

    prop = p_new(xml_prop_t, 1);
    /* Parse tag name */
    name = p;
    while (len) {
        if (!isalnum(*p)) {
            break;
        }
        len--;
        p++;
    }
    if (len <= 0) {
        goto error;
    }
    prop->name = p_new(char, p - name + 1);
    pstrcpylen(prop->name, p - name + 1, name, p - name);

    SKIPSPACES(p, len);
    ENSURE(p, len, '=');

    p++;
    len--;
    if (len <= 0) {
        goto error;
    }

    if (*p != '\'' && *p != '"') {
        goto error;
    }
    quot = *p;
    p++;
    len--;

    value = p;
    while (len > 0) {
        if ((quot && *p == quot)) {
            break;
        }
        if (*p == '\\') {
            p++;
            len--;
        }
        p++;
        len--;
    }

    if (len <= 0) {
        goto error;
    }

    unquoted_len = strconv_unquote(NULL, 0, value, p - value);
    prop->value = p_new(char, unquoted_len + 1);
    strconv_unquote(prop->value, unquoted_len + 1, value, p - value);

    if (quot) {
        p++;
        len--;
    }

    if (len <= 0) {
        goto error;
    }

end:
    if (prop) {
        ret = PARSE_PROP;
    } else {
        ret = PARSE_END_PROP;
    }

    if (dst) {
        *dst = prop;
    } else {
        xml_prop_t_delete(&prop);
    }
    if (pend) {
        *pend = p;
    }

    return ret;

error:
    if (pend) {
        *pend = p;
    }
    xml_prop_t_delete(&prop);
    return PARSE_ERROR;
}

/* Parse the first tag in the payload, as well as the associated text
 * that immediately follows it. Exit with PARSE_TAG in this case.
 * If the tag found is "</(*endtag)>", exit with PARSE_END_TAG without
 * creating any new tag.
 * Exit with PARSE_EOF if the end of the buffer is found.
 * In all other cases, exit with PARSE_ERROR.
 */
static parse_t xml_get_tag(xml_tag_t **dst, const char *payload, size_t len,
                           const char *endtag, const char **pend)
{
    xml_tag_t *tag = NULL;
    xml_prop_t *prop = NULL;
    xml_prop_t **t = NULL;
    const char *p = payload;
    const char *name, *nameend, *mypend, *text, *textend;
    bool closing;
    parse_t ret;

    if (!dst) {
        return PARSE_ERROR;
    }
    SKIPSPACES(p, len);
    if (len <= 0) {
        return PARSE_EOF;
    }
    /* We're supposed to get a tag now... */
    ENSURE(p, len, '<');
    p++;
    len--;

    /* Skip spaces between '<' and tag name */
    SKIPSPACES(p, len);
    if (len <= 0) {
        goto error;
    }

    /* Is this the end of the current tag ? */
    if (*p == '/') {
        closing = true;
        p++;
        len--;
    } else {
        closing = false;
    }

    /* Parse tag name */
    name = p;
    while (len) {
        if (!isalnum(*p) && *p != ':') {
            break;
        }
        len--;
        p++;
    }
    if (len <= 0) {
        goto error;
    }
    nameend = p;

    if (closing) {
        if (!endtag || !strstart(name, endtag, NULL)) {
            goto error;
        }
        /* tag end : eat trailing spaces and '>' */
        SKIPSPACES(p, len);
        ENSURE(p, len, '>');
        len--;
        p++;
        if (pend) {
            *pend = p;
        }
        return PARSE_END_TAG;
    }

    tag = p_new(xml_tag_t, 1);
    tag->fullname = p_new(char, nameend - name + 1);
    pstrcpylen(tag->fullname, nameend - name + 1, name, nameend - name);
    tag->name = strchr(tag->fullname, ':');
    if (!tag->name) {
        tag->name = tag->fullname;
    }

    t = &tag->property;
    while (len > 0 && *p != '>') {
        /* tag name and properties are separated by some space */
        if (!isspace(*p)) {
            goto error;
        }
        SKIPSPACES(p, len);
        if (len <= 0) {
            goto error;
        }

        /* now, parse property */
        switch (ret = xml_get_prop(&prop, p, len, &mypend)) {
          case PARSE_PROP:
            *t = prop;
            prop->next = NULL;
            t = &prop->next;
            break;
          case PARSE_END_PROP:
            break;
          default:
            goto error;
        }
        len -= mypend - p;
        p = mypend;
        if (ret == PARSE_END_PROP) {
            break;
        }
    }
    if (len <= 0) {
        goto error;
    }
    /* Skip '>' */
    p++;
    len--;

    /* Locate text */
    SKIPSPACES(p, len);
    text = p;
    textend = p;
    while (len > 0 && *p != '<') {
        if (!isspace(*p)) {
            textend = p;
        }
        p++;
        len--;
    }
    if (len <= 0) {
        goto error;
    }
    if (text != p) {
        textend++;
        tag->text = p_new(char, textend - text + 1);
        pstrcpylen(tag->text, textend - text + 1, text, textend - text);
    }

    if (dst) {
        *dst = tag;
    } else {
        xml_tag_t_delete(&tag);
    }

    if (pend) {
        *pend = p;
    }
    return PARSE_TAG;
error:
    if (pend) {
        *pend = p;
    }
    xml_tag_t_delete(&tag);
    return PARSE_ERROR;
}

/* Parse an XML buffer and put it into an xml_tag_t.
 */
static int xml_parse(xml_tag_t *dst, const char *payload, size_t payload_len,
                     const char **pend)
{
    xml_tag_t *next;
    xml_tag_t **babyp;
    const char *mypend;

    mypend = payload;

    babyp = &dst->child;

    for (;;) {
        switch (xml_get_tag(&next, payload, payload_len, dst->fullname,
                            &mypend)) {
          case PARSE_TAG:
            payload_len -= mypend - payload;
            payload = mypend;
            next->parent = dst;
            *babyp = next;
            babyp = &(*babyp)->next;
            if (xml_parse(next, payload, payload_len, &mypend)) {
                if (pend) {
                    *pend = mypend;
                }
                return -1;
            }
            payload_len -= mypend - payload;
            payload = mypend;
            break;
          case PARSE_END_TAG:
            if (pend) {
                *pend = mypend;
            }
            return 0;
          case PARSE_EOF:
            if (!dst->fullname) {
                if (pend) {
                    *pend = mypend;
                }
                return 0;
            }
            /* FALLTHROUGH */
          default:
            if (pend) {
                *pend = mypend;
            }
            return -1;
        }
    }
}

int xml_get_xml_tag(xml_tree_t *tree, const char *payload, size_t len,
                    const char **pend);
int xml_get_xml_tag(xml_tree_t *tree, const char *payload, size_t len,
                    const char **pend)
{
    const char *mypend;

    SKIPSPACES(payload, len);
    mypend = payload;
    if (strstart(payload, "<?xml", &mypend)) {
        len -= mypend - payload;
        payload = mypend;
        mypend = memsearch(payload, len, "?>", 2);
        if (!mypend) {
            return 1;
        }
        mypend += strlen("?>");
    }
    if (pend) {
        *pend = mypend;
    }
    return 0;
}

/* Create a new XML tree from an 
 */
xml_tree_t *xml_new_tree(const char *payload, size_t len)
{
    xml_tree_t *tree;
    const char *pend = payload;

    tree = p_new(xml_tree_t, 1);
    tree->root = p_new(xml_tag_t, 1);
    if (xml_get_xml_tag(tree, payload, len, &pend)) {
    }
    len -= pend - payload;
    payload = pend;
    if (xml_parse(tree->root, payload, len, NULL)) {
        xml_delete_tree(&tree);
        return NULL;
    }
    return tree;
}

void xml_delete_tree(xml_tree_t **tree)
{
    if (tree) {
        if ((*tree)->root) {
            xml_tag_t_list_wipe(&(*tree)->root);
            p_delete(&(*tree)->root);
        }
        p_delete(tree);
    }
}

static void xml_tree_dump(const xml_tag_t *root, const char *prefix)
{
    char newprefix[30];
    char buf[128];
    const xml_prop_t *prop;
    if (!root) {
        return;
    }
    if (root->fullname) {
        fprintf(stderr, "%s <%s", prefix, root->fullname);
        fprintf(stderr, " addr=\"%p\" parent=\"%p\" next=\"%p\"",
                root, root->parent, root->next);
        for (prop = root->property; prop; prop = prop->next) {
            strconv_quote(buf, sizeof(buf), prop->value, strlen(prop->value), '"');
            fprintf(stderr, " %s=\"%s\"", prop->name, buf);
        }
        fprintf(stderr, ">\n");
    }
    if (root->text) {
        fprintf(stderr, "%s\n", root->text);
    }
    if (root->child) {
        snprintf(newprefix, sizeof(newprefix), "%s   ", prefix);
        xml_tree_dump(root->child, newprefix);
    }
    if (root->fullname) {
        fprintf(stderr, "%s </%s>\n", prefix, root->fullname);
    }

    if (root->next) {
        //fprintf(stderr, "DUMPING %p->next=%p\n", root, root->next);
        xml_tree_dump(root->next, prefix);
    }
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>
#include "blob.h"

START_TEST(check_xmlparse)
{
    blob_t blob;
    xml_tree_t *tree;

    blob_init(&blob);

    fail_if(blob_append_file_data(&blob, "samples/simple.xml") < 0,
            "unable to read sample file");
    tree = xml_new_tree(blob_get_cstr(&blob), blob.len);
    fail_if(!tree, "simple");
    if (tree) {
        xml_tree_dump(tree->root, "root:");
        xml_delete_tree(&tree);
    }

    blob_wipe(&blob);
}
END_TEST

Suite *check_xml_suite(void)
{
    Suite *s  = suite_create("XML");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_xmlparse);
    return s;
}
#endif

