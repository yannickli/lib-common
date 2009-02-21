/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
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

static char *xml_dupstr_mp(xml_tree_t *tree, const char *src, int len)
{
    if (src) {
        return mp_dupstr(tree->mp, src, len);
    }
    return mp_new(tree->mp, char, len + 1);
}

#define xml_deletestr_mp(p)   (*(p) = NULL)
#define xml_prop_delete(p)    (*(p) = NULL)
#define xml_tag_delete(p)     (*(p) = NULL)

static char const alnumto6bits[256] = {
    /* 0xFF means we do not care about this char.
     * Only a-z,A-Z,0-9, '-' and '_' are really meaningfull to us in
     * this context.
     * */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, /*      -   */
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, /* 01234567 */
    0x09, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* 89       */
    0xFF, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, /*  ABCDEFG */
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, /* HIJKLMNO */
    0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, /* PQRSTUVW */
    0x22, 0x23, 0x24, 0xFF, 0xFF, 0xFF, 0xFF, 0x25, /* XYZ    _ */
    0xFF, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, /*  abcdefg */
    0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, /* hijklmno */
    0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, /* pqrstuvw */
    0x3D, 0x3E, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* xyz      */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /*          */
};

static int xml_hash(const char *str, int len)
{
    int ret = 0xA5C39600;
    int key;

    if (len < 0) {
        len = strlen(str);
    }

#define RIGHT_ROTATE_32(val, n) \
    (((unsigned int)(val) >> (n)) | ((unsigned int)(val) << (32 - (n))))
    while (len) {
        key = alnumto6bits[(unsigned char)(*str)];
        ret = RIGHT_ROTATE_32(ret, 3);
        ret ^= key;
        str++;
        len--;
    }
    return ret;
}

void xml_tree_wipe(xml_tree_t *tree)
{
    mem_fifo_pool_delete(&tree->mp);
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

/*  unescape XML entities, returns decoded length, or -1 on error
 * The resulting string is not NUL-terminated
 **/
static int strconv_xmlunescape(char *str, int len)
{
    char *wpos = str, *start, *end;
    int res_len = 0;

    if (len < 0) {
        len = strlen(str);
    }

    while (len > 0) {
        char c;

        c = *str++;
        len--;

        if (c == '&') {
            int val;

            start = str;
            end = memchr(str, ';', len);
            if (!end) {
                e_trace(1, "Could not find entity end (%.*s%s)",
                        5, str, len > 5 ? "..." : "");
                goto error;
            }
            *end = '\0';

            c = *str;
            if (c == '#') {
                str++;
                if (*str == 'x') {
                    /* hexadecimal */
                    str++;
                    val = strtol(str, (const char **)&str, 16);
                } else {
                    /* decimal */
                    val = strtol(str, (const char **)&str, 10);
                }
                if (str != end) {
                    e_trace(1, "Bad numeric entity, got trailing char: '%c'", *str);
                    goto error;
                }
            } else {
                /* TODO: Support externally defined entities */
                /* textual entity */
                switch (c) {
                  case 'a':
                    if (!strcmp(str, "amp")) {
                        val = '&';
                    } else
                    if (!strcmp(str, "apos")) {
                        val = '\'';
                    } else {
                        e_trace(1, "Unsupported entity: %s", str);
                        goto error;
                    }
                    break;

                  case 'l':
                    if (!strcmp(str, "lt")) {
                        val = '<';
                    } else {
                        e_trace(1, "Unsupported entity: %s", str);
                        goto error;
                    }
                    break;

                  case 'g':
                    if (!strcmp(str, "gt")) {
                        val = '>';
                    } else {
                        e_trace(1, "Unsupported entity: %s", str);
                        goto error;
                    }
                    break;

                  case 'q':
                    if (!strcmp(str, "quot")) {
                        val = '"';
                    } else {
                        e_trace(1, "Unsupported entity: %s", str);
                        goto error;
                    }
                    break;

                  default:
                    /* invalid entity */
                    e_trace(1, "Unsupported entity: %s", str);
                    goto error;
                }
            }

            /* write unicode char */
            {
                int bytes = __pstrputuc(wpos, val);

                wpos += bytes;
                res_len += bytes;
            }

            str = end + 1;
            len -= str - start;
            continue;
        }

        *wpos++ = c;
        res_len++;
    }

    return res_len;

  error:
    /* Cut string */
    *wpos = '\0';
    return -1;
}

static int str_scanhexdigits(const char *dp, int min, int max, int *cp)
{
    int val = 0;
    int i, digit;

    for (i = 0; i < max; i++) {
        if ((digit = str_digit_value(dp[i])) < 16) {
            val = (val << 4) + digit;
        } else {
            break;
        }
    }
    if (i < min) {
        return 0;
    } else {
        *cp = val;
        return i;
    }
}

static int strconv_unquote(char *dest, int size, const char *src, int len)
{
    int i, j;

    if (len < 0) {
        len = strlen(src);
    }

    for (i = j = 0; i < len;) {
        int c = src[i];

        i += 1;
        if (c == '\\' && i < len) {
            c = src[i];
            i += 1;
            switch (c) {
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'v':
                c = '\v';
                break;
            case 'b':
                c = '\b';
                break;
            case 'a':
                c = '\a';
                break;
            case '0'...'7':
                c = c - '0';
                if (i < len && src[i] >= '0' && src[i] <= '7') {
                    c = (c << 3) + src[i] - '0';
                    i++;
                    if (c < '\040' && i < len
                    &&  src[i] >= '0' && src[i] <= '7') {
                        c = (c << 3) + src[i] - '0';
                        i++;
                    }
                }
                break;
            case 'u':
                if (i + 4 <= len && str_scanhexdigits(src + i, 4, 4, &c))
                    i += 4;
                break;
            case 'x':
                if (i + 2 <= len && str_scanhexdigits(src + i, 2, 2, &c))
                    i += 2;
                break;
            default:
                break;
            }
        }
        if (j < size) {
            dest[j] = c;
        }
        j++;
    }
    if (j < size) {
        dest[j] = '\0';
    }
    return j;
}

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
    int unquoted_len, decoded_len;
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
    prop->name = xml_dupstr_mp(tree, name, p - name);
    prop->name_hash = xml_hash(prop->name, p - name);

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

    unquoted_len = strconv_unquote(NULL, 0, value, p - value);
    prop->value = xml_dupstr_mp(tree, NULL, unquoted_len);
    strconv_unquote(prop->value, unquoted_len + 1, value, p - value);

    decoded_len = strconv_xmlunescape(prop->value, unquoted_len);
    if (decoded_len >= 0) {
        prop->value[decoded_len] = '\0';
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
    tag->fullname = xml_dupstr_mp(tree, name, nameend - name);
    tag->name = strchr(tag->fullname, ':');
    if (tag->name) {
        /* Skip ':' */
        tag->name++;
    } else {
        tag->name = tag->fullname;
    }
    tag->name_hash = xml_hash(tag->name, -1);

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
        int decoded_len;

        textend++;
        tag->text = xml_dupstr_mp(tree, text, textend - text);
        decoded_len = strconv_xmlunescape(tag->text, textend - text);
        if (decoded_len >= 0) {
            tag->text[decoded_len] = '\0';
        }
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
    xml_tag_delete(&tag);
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
    tree->mp = mem_fifo_pool_new(2 * len);

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
    int patlen, skip;
    const char *p;
    const xml_tag_t *cur, *tmp;
    int matchall;
    int pattern_hash;

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

    pattern_hash = xml_hash(pattern, patlen);
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

void blob_append_branch(const xml_tag_t *root, blob_t *blob,
                        const char *prefix)
{
    /* OG: should pass indentation count instead of prefix string */
    char newprefix[32];
    const xml_prop_t *prop;
    const xml_tag_t *cur;

    if (!root)
        return;

    if (root->fullname) {
        blob_append_cstr(blob, prefix);
        blob_append_cstr(blob, "<");
        blob_append_cstr(blob, root->fullname);
        for (prop = root->property; prop; prop = prop->next) {
            blob_append_cstr(blob, " ");
            blob_append_cstr(blob, prop->name);
            blob_append_cstr(blob, "=\"");
            sb_adds_xmlescape(blob, prop->value);
            blob_append_cstr(blob, "\"");
        }
        blob_append_cstr(blob, ">\n");
    }
    snprintf(newprefix, sizeof(newprefix), "%s   ", prefix);
    if (root->text) {
        blob_append_cstr(blob, newprefix);
        blob_append_cstr(blob, root->text);
        blob_append_cstr(blob, "\n");
    }

    for (cur = root->child; cur; cur = cur->next) {
        blob_append_branch(cur, blob, newprefix);
    }
    if (root->fullname) {
        blob_append_cstr(blob, prefix);
        blob_append_cstr(blob, "</");
        blob_append_cstr(blob, root->fullname);
        blob_append_cstr(blob, ">\n");
    }
}

void blob_append_tree(const xml_tree_t *tree, blob_t *blob)
{
     blob_append_branch(tree->root, blob, "");
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
    blob_t blob;
    xml_tree_t *tree;
    const xml_tag_t *tag, *tag2, *tag3, *tag4;
    int verbose = 0;
    char error_buf[256];

    error_buf[0] = '\0';
    blob_init(&blob);

    fail_if(blob_append_file_data(&blob, "samples/simple.xml") < 0,
            "unable to read sample file");
    tree = xml_new_tree(blob_get_cstr(&blob), blob.len,
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

    blob_wipe(&blob);
}
END_TEST

START_TEST(check_xmlhash)
{
    fail_if(xml_hash("Intersec", -1) == xml_hash("MMSC", -1), "Collision");
    fail_if(xml_hash("Intersec", -1) == xml_hash("Intersce", -1), "Collision");
    fail_if(xml_hash("Intersec", -1) == xml_hash("Intersed", -1), "Collision");
}
END_TEST

START_TEST(check_str_xmlunescape)
{
    int res;
    char dest[BUFSIZ];

    const char *encoded = "3&lt;=8: Toto -&gt; titi &amp; "
        "t&#xE9;t&#232; : &quot;you&apos;re&quot;";
    const char *decoded = "3<=8: Toto -> titi & tétè : \"you're\"";
    const char *badencoded = "3&lt;=8: Toto -&tutu; titi &amp; "
        "t&#xE9;t&#232; : &quot;you&apos;re&quot;";
    const char *baddecoded = "3<=8: Toto -";

    pstrcpy(dest, sizeof(dest), encoded);

    res = strconv_xmlunescape(dest, -1);
    if (res >= 0) {
        dest[res] = '\0';
    }
    fail_if(res != sstrlen(decoded), "strconv_xmlunescape returned bad"
            "length: expected %zd, got %d. (%s)", strlen(decoded), res, dest);
    fail_if(strcmp(dest, decoded), "strconv_xmlunescape failed decoding");

    pstrcpy(dest, sizeof(dest), badencoded);

    res = strconv_xmlunescape(dest, -1);
    fail_if(res != -1, "strconv_xmlunescape did not detect error");
    fail_if(strcmp(dest, baddecoded), "strconv_xmlunescape failed to clean "
            "badly encoded str");
}
END_TEST


Suite *check_xml_suite(void)
{
    Suite *s  = suite_create("XML");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_xmlparse);
    tcase_add_test(tc, check_xmlhash);
    tcase_add_test(tc, check_str_xmlunescape);
    return s;
}
#endif
