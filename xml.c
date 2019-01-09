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

#include "xml.h"
#include "xmlpp.h"
#include "hash.h"

void xml_tree_wipe(xml_tree_t *tree)
{
    mem_stack_pool_delete(&tree->mp);
}

typedef enum parse_t {
    PARSE_EOF,
    PARSE_START_TAG,
    PARSE_END_TAG,
    PARSE_ADD_TAG,
    PARSE_PROP,
    PARSE_END_PROP,
    PARSE_ERROR,
} parse_t;

#define SKIPSPACES(p, len)                       \
    while (len) {                                \
        if (!isspace((unsigned char)*(p))) {     \
            break;                               \
        }                                        \
        (len)--;                                 \
        (p)++;                                   \
    }
#define ENSURE(p, len, c)                                 \
    do {                                                  \
        if ((len) <= 0 || *(p) != (c)) {                  \
            if (error_buf) {                              \
                snprintf(error_buf, buf_len,              \
                    "Expected %c but found %c", c, *(p)); \
            }                                             \
            goto error;                                   \
        }                                                 \
    } while (0)

/* Parse a property inside a tag and put it in (*dst).
 * Property is of form : prop = "value"
 * Single/Double quoting is supported.
 */
static parse_t xml_get_prop(xml_tree_t *tree, xml_prop_t **dst,
                            const char *payload, size_t len,
                            const char **pend, const char *tag_name,
                            char *error_buf, size_t buf_len)
{
    const char *name, *value;
    const char *p;
    xml_prop_t *prop;
    char quot;
    parse_t ret;

    prop = NULL;
    p = payload;

    SKIPSPACES(p, len);

    if (len <= 0) {
        goto end;
    }
    if (*p == '>' || *p == '/') {
        goto end;
    }

    prop = mp_new(tree->mp, xml_prop_t, 1);
    /* Parse tag name */
    name = p;
    while (len) {
        if (!isalnum((unsigned char)*p) && *p != ':' && *p != '-') {
            break;
        }
        len--;
        p++;
    }
    if (len <= 0) {
        if (error_buf) {
            snprintf(error_buf, buf_len,
                     "Unexpected end when parsing property %s on tag %s",
                     name, tag_name);
        }
        goto error;
    }
    prop->name = mp_dupz(tree->mp, name, p - name);
    prop->name_hash = hsieh_hash(prop->name, p - name);

    SKIPSPACES(p, len);
    ENSURE(p, len, '=');

    p++;
    len--;
    if (len <= 0) {
        goto error;
    }

    SKIPSPACES(p, len);
    if (*p != '\'' && *p != '"') {
        if (error_buf) {
            snprintf(error_buf, buf_len,
                     "Missing quote for property %s on tag %s",
                     prop->name, tag_name);
        }
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
        if (error_buf) {
            snprintf(error_buf, buf_len,
                     "Value of property %s starts with %c,"
                     " but matching %c not found on tag %s",
                     prop->name, quot, quot, tag_name);
        }
        goto error;
    }

    /* XXX: quick and dirty hack for fast copyless xml parsing!
     * The unescaped output will be at most the same size as the
     * source, thus we can allocate pool memory for the source size and
     * wrap that in an sb_t that should not get reallocated by
     * sb_add_xmlunescape.  We probably should prevent
     * sb_add_xmlunescape from reallocating memory altogether,
     * but sb_t do not provide a way to do that yet.
     */
    prop->value = mp_new(tree->mp, char, p - value + 1);
    {
        sb_t sb;

        sb_init_full(&sb, prop->value, 0, p - value + 1, MEM_STATIC);
        if (sb_add_xmlunescape(&sb, value, p - value)) {
            snprintf(error_buf, buf_len,
                     "Unable to unescape property value %s of tag %s",
                     prop->name, tag_name);
            goto error;
        }
        sb_wipe_not_needed(&sb);
    }

    if (quot) {
        p++;
        len--;
    }

    if (len <= 0) {
        if (error_buf) {
            snprintf(error_buf, buf_len,
                     "Unexpected end of buf, when parsing property %s on tag %s",
                     prop->name, tag_name);
        }
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
    }
    if (pend) {
        *pend = p;
    }

    return ret;

error:
    if (pend) {
        *pend = p;
    }
    return PARSE_ERROR;
}

/* Parse the first tag in the payload, as well as the associated text
 * that immediately follows it. Exit with PARSE_TAG in this case.
 * If the tag found is "</(*endtag)>", exit with PARSE_END_TAG without
 * creating any new tag.
 * Exit with PARSE_EOF if the end of the buffer is found.
 * In all other cases, exit with PARSE_ERROR.
 */
static parse_t xml_get_tag(xml_tree_t *tree, xml_tag_t **dst,
                           const char *payload, size_t len,
                           const char *endtag, const char **pend,
                           char *error_buf, size_t buf_len)
{
    xml_tag_t *tag = NULL;
    xml_prop_t *prop = NULL;
    xml_prop_t **t = NULL;
    const char *p = payload;
    const char *name, *nameend, *mypend, *text, *textend;
    bool closing;
    parse_t parse, ret;

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

    /* Is this a closing tag ? */
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
        if (!isalnum((unsigned char)*p) && *p != ':' && *p != '-' && *p != '_') {
            break;
        }
        len--;
        p++;
    }
    if (len <= 0) {
        if (error_buf) {
            snprintf(error_buf, buf_len,
                     "Error parsing tag name");
        }
        goto error;
    }
    nameend = p;

    if (closing) {
        if (!endtag) {
            if (error_buf) {
                snprintf(error_buf, buf_len,
                         "Unexpected closing tag '%.*s'",
                         (int)(nameend - name), name);
            }
            goto error;
        }
        if (!strstart(name, endtag, NULL)) {
            if (error_buf) {
                snprintf(error_buf, buf_len,
                         "End tag mismatch, search '%.*s' but found '%s'",
                         (int)(nameend - name), name, endtag);
            }
            goto error;
        }
        /* tag end: eat trailing spaces and '>' */
        SKIPSPACES(p, len);
        ENSURE(p, len, '>');
        len--;
        p++;
        if (pend) {
            *pend = p;
        }
        return PARSE_END_TAG;
    }

    tag = mp_new(tree->mp, xml_tag_t, 1);
    tag->fullname = mp_dupz(tree->mp, name, nameend - name);
    tag->name = strchr(tag->fullname, ':');
    if (tag->name) {
        /* Skip ':' */
        tag->name++;
    } else {
        tag->name = tag->fullname;
    }
    tag->name_hash = hsieh_hash(tag->name, -1);

    t = &tag->property;
    /* tag name and properties are separated by some space */
    if (!isspace((unsigned char)*p) && *p != '/' && *p != '>') {
        if (error_buf) {
            snprintf(error_buf, buf_len,
                "Unexpected char on tag %s", tag->name);
        }
        goto error;
    }
    while (len > 0 && *p != '>' && *p != '/') {
        /* now, parse property */
        switch (parse = xml_get_prop(tree, &prop, p, len, &mypend, tag->name,
                                     error_buf, buf_len))
        {
          case PARSE_PROP:
            *t = prop;
            prop->next = NULL;
            t = &prop->next;
            break;
          case PARSE_END_PROP:
            break;
          case PARSE_ERROR:
            goto error;
          default:
            if (error_buf) {
                snprintf(error_buf, buf_len,
                         "Error with properties on %s", tag->name);
            }
            goto error;
        }
        len -= mypend - p;
        p = mypend;
        SKIPSPACES(p, len);
        if (len <= 0) {
            if (error_buf) {
                snprintf(error_buf, buf_len,
                         "Unexpected end on tag %s", tag->name);
            }
            goto error;
        }

        if (parse == PARSE_END_PROP) {
            break;
        }
    }
    if (len <= 0) {
        if (error_buf) {
            snprintf(error_buf, buf_len,
                     "Unexpected end on tag %s", tag->name);
        }
        goto error;
    }
    if (*p == '/') {
        p++;
        len--;
        SKIPSPACES(p, len);
        ENSURE(p, len, '>');
        /* Skip '>' */
        p++;
        len--;
        ret = PARSE_ADD_TAG;
        goto end;
    } else {
        /* Skip '>' */
        p++;
        len--;
        ret = PARSE_START_TAG;
    }

    /* Locate text */
    SKIPSPACES(p, len);
    text = p;
    textend = p;
    while (len > 0 && *p != '<') {
        if (!isspace((unsigned char)*p)) {
            textend = p;
        }
        p++;
        len--;
    }
    if (len <= 0) {
        if (error_buf) {
            snprintf(error_buf, buf_len,
                     "Unexpected end on tag %s", tag->name);
        }
        goto error;
    }
    if (text != p) {
        /* XXX: same remark as above, allocation of converted space is
         * conservative and sb simply wraps allocated destination buffer.
         */
        sb_t sb;

        textend++;
        tag->text = mp_new(tree->mp, char, textend - text + 1);
        sb_init_full(&sb, tag->text, 0, textend - text + 1, MEM_STATIC);
        if (sb_add_xmlunescape(&sb, text, textend - text)) {
            snprintf(error_buf, buf_len,
                     "Unable to decode text value of tag %s", tag->name);
            goto error;
        }
        sb_wipe_not_needed(&sb);
    }

end:
    if (dst) {
        *dst = tag;
    }

    if (pend) {
        *pend = p;
    }
    return ret;
error:
    if (pend) {
        *pend = p;
    }
    return PARSE_ERROR;
}

/* Parse an XML buffer and put it into an xml_tag_t. */
static int xml_parse(xml_tree_t *tree, xml_tag_t *dst,
                     const char *payload, size_t payload_len,
                     const char **pend, char *error_buf, size_t buf_len)
{
    xml_tag_t *next;
    xml_tag_t **babyp;
    const char *mypend;

    mypend = payload;

    babyp = &dst->child;

    for (;;) {
        switch (xml_get_tag(tree, &next, payload, payload_len, dst->fullname,
                            &mypend, error_buf, buf_len)) {
          case PARSE_ADD_TAG:
            payload_len -= mypend - payload;
            payload = mypend;
            next->parent = dst;
            *babyp = next;
            babyp = &(*babyp)->next;
            break;
          case PARSE_START_TAG:
            payload_len -= mypend - payload;
            payload = mypend;
            next->parent = dst;
            *babyp = next;
            babyp = &(*babyp)->next;
            if (xml_parse(tree, next, payload, payload_len, &mypend,
                          error_buf, buf_len))
            {
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
            if (error_buf) {
                snprintf(error_buf, buf_len,
                         "Unexpected end of file when parsing %s",
                         dst->fullname);
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

static int xml_get_xml_tag(xml_tree_t *tree, const char *payload, size_t len,
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

/* Create a new XML tree from a buffer */
xml_tree_t *xml_new_tree(const char *payload, size_t len,
                         char *error_buf, size_t buf_len)
{
    xml_tree_t *tree;
    const char *pend = payload;

    tree = p_new(xml_tree_t, 1);
    tree->mp = mem_stack_pool_new(2 * len);

    tree->root = mp_new(tree->mp, xml_tag_t, 1);
    if (xml_get_xml_tag(tree, payload, len, &pend)) {
        xml_tree_delete(&tree);
        return NULL;
    }
    len -= pend - payload;
    payload = pend;
    if (xml_parse(tree, tree->root, payload, len, NULL,
                  error_buf, buf_len))
    {
        xml_tree_delete(&tree);
        return NULL;
    }
    return tree;
}

const xml_tag_t *xml_search_branch(const xml_tag_t *branch,
                                   const xml_tag_t **previous,
                                   const char *pattern)
{
    int matchall, patlen, skip;
    const char *p;
    const xml_tag_t *cur, *tmp;
    uint32_t pattern_hash;

    p = strchr(pattern, '/');
    if (p) {
        patlen = p - pattern;
        skip = patlen + 1;
    } else {
        patlen = strlen(pattern);
        skip = patlen;
    }

    if (patlen <= 0) {
        if (!previous || *previous == NULL) {
            return branch;
        } else
        if (*previous == branch) {
            *previous = NULL;
            return NULL;
        }
        return NULL;
    }

    pattern_hash = hsieh_hash(pattern, patlen);
    matchall = (patlen == 1 && pattern[0] == '*');

    for (cur = branch->child; cur; cur = cur->next) {
        /* XXX: Compare to name and not fullname (ie: ignore namespaces) */
        if (matchall || (cur->name_hash == pattern_hash
                         && !strncmp(pattern, cur->name, patlen))) {
            tmp = xml_search_branch(cur, previous, pattern + skip);
            if (tmp) {
                return tmp;
            }
        }
    }

    return NULL;
}

const xml_tag_t* xml_search_subtree(const xml_tree_t *tree,
                                    const xml_tag_t *subtree,
                                    const xml_tag_t *previous,
                                    const char *pattern)
{
    if (!tree || !tree->root || !pattern || !subtree) {
        return NULL;
    }
    /* XXX: Only support absolute path for now */
    if (pattern[0] != '/') {
        return NULL;
    }
    pattern++;

    return xml_search_branch(subtree, &previous, pattern);
}

static void xml_pp_tag(xmlpp_t *pp, const xml_tag_t *tag)
{
    if (!tag)
        return;

    if (tag->fullname) {
        xmlpp_opentag(pp, tag->fullname);
        for (xml_prop_t *prop = tag->property; prop; prop = prop->next) {
            xmlpp_putattr(pp, prop->name, prop->value);
        }
    }
    if (tag->text)
        xmlpp_puts(pp, tag->text);

    for (xml_tag_t *cur = tag->child; cur; cur = cur->next) {
        xml_pp_tag(pp, cur);
    }
    if (tag->fullname) {
        xmlpp_closetag(pp);
    }
}

void sb_add_xmltag(sb_t *sb, const xml_tag_t *root)
{
    xmlpp_t pp;

    xmlpp_open(&pp, sb);
    xml_pp_tag(&pp, root);
    xmlpp_close(&pp);
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

static void xml_branch_dump(const xml_tag_t *root, const char *prefix)
{
    char newprefix[30];
    SB_1k(buf);
    const xml_prop_t *prop;
    const xml_tag_t *cur;

    if (!root) {
        return;
    }
    if (root->fullname) {
        fprintf(stderr, "%s <%s", prefix, root->fullname);
        fprintf(stderr, " addr=\"%p\" parent=\"%p\" next=\"%p\"",
                root, root->parent, root->next);
        for (prop = root->property; prop; prop = prop->next) {
            sb_reset(&buf);
            sb_adds_xmlescape(&buf, prop->value);
            fprintf(stderr, " %s=\"%s\"", prop->name, buf.data);
        }
        fprintf(stderr, ">\n");
    }
    if (root->text) {
        fprintf(stderr, "%s\n", root->text);
    }

    snprintf(newprefix, sizeof(newprefix), "%s   ", prefix);
    for (cur = root->child; cur; cur = cur->next) {
        xml_branch_dump(cur, newprefix);
    }
    if (root->fullname) {
        fprintf(stderr, "%s </%s>\n", prefix, root->fullname);
    }
    sb_wipe(&buf);
}

/* OG: should include samples/simple.xml here and create temporary file
 * for test purposes
 */

START_TEST(check_xmlparse)
{
    sb_t sb;
    xml_tree_t *tree;
    const xml_tag_t *tag, *tag2, *tag3, *tag4;
    int verbose = 0;
    char error_buf[256];

    error_buf[0] = '\0';
    sb_init(&sb);

    fail_if(sb_read_file(&sb, "samples/simple.xml") < 0,
            "unable to read sample file");
    tree = xml_new_tree(sb.data, sb.len,
                        error_buf, sizeof(error_buf));
    fail_if(!tree, "simple, %s", error_buf);
    if (tree) {
        tag = xml_search(tree, NULL, "/");
        fail_if(tag != tree->root, "search for root failed");

        tag = xml_search(tree, NULL, "/toto");
        fail_if(tag != NULL, "search for toto succedded");

        tag = xml_search(tree, NULL, "/part1");
        fail_if(!tag || strcmp(tag->name, "part1"), "search for part1 failed");

        tag = xml_search(tree, NULL, "/part2");
        fail_if(!tag || strcmp(tag->name, "part2"), "search for part2 failed");

        tag = xml_search(tree, NULL, "/part3/title");
        fail_if(!tag || strcmp(tag->name, "title"), "search for title failed");

        tag = xml_search(tree, NULL, "/part3");
        fail_if(!tag || strcmp(tag->name, "part3"), "search for part3 failed");

        tag = xml_search_subtree(tree, tag, NULL, "/title");
        fail_if(!tag || strcmp(tag->name, "title"), "search for title failed");

        tag = xml_search(tree, NULL, "/part3/chapter1/paragraph");
        fail_if(!tag, "search for paragraph1 failed");
        if (tag && verbose) {
            xml_branch_dump(tag, "paragraph1:");
        }

        tag2 = xml_search(tree, tag, "/part3/chapter1/paragraph");
        fail_if(!tag2, "search for paragraph2 failed");
        if (tag2 && verbose) {
            xml_branch_dump(tag2, "paragraph2:");
        }

        tag3 = xml_search(tree, tag2, "/part3/chapter1/paragraph");
        fail_if(tag3, "search for paragraph3 failed");

        tag4 = xml_search(tree, NULL, "/part3/*/paragraph");
        fail_if(tag4 != tag, "search for first paragraph failed");

        tag = xml_search(tree, NULL, "/part4");
        fail_if(!tag || strcmp(tag->name, "part4"), "search for part4 failed");

        if (tree->root && verbose) {
            xml_branch_dump(tree->root, "root:");
        }
        xml_tree_delete(&tree);
    }

    sb_wipe(&sb);
}
END_TEST

START_TEST(check_str_xmlunescape)
{
    int res;
    SB_8k(sb);

    const char *encoded = "3&lt;=8: Toto -&gt; titi &amp; "
        "t&#xE9;t&#232; : &quot;you&apos;re&quot;";
    const char *decoded = "3<=8: Toto -> titi & tétè : \"you're\"";
    const char *badencoded = "3&lt;=8: Toto -&tutu; titi &amp; "
        "t&#xE9;t&#232; : &quot;you&apos;re&quot;";

    res = sb_adds_xmlunescape(&sb, encoded);
    fail_if(sb.len != sstrlen(decoded), "strconv_xmlunescape returned bad"
            "length: expected %zd, got %d. (%s)", strlen(decoded),
            sb.len, sb.data);
    fail_if(strcmp(sb.data, decoded), "strconv_xmlunescape failed decoding");

    sb_reset(&sb);
    res = sb_adds_xmlunescape(&sb, badencoded);
    fail_if(res != -1, "strconv_xmlunescape did not detect error");
    sb_wipe(&sb);
}
END_TEST


Suite *check_xml_suite(void)
{
    Suite *s  = suite_create("XML");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_str_xmlunescape);
    tcase_add_test(tc, check_xmlparse);
    return s;
}
#endif
