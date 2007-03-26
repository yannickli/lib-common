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

#include <lib-common/mem.h>
#include <lib-common/blob.h>
#include <lib-common/macros.h>
#include <lib-common/string_is.h>

#include "conf.h"

/* TODO:
 * - test, debug...
 * - keep comments
 */

#define CONF_DBG_LVL 3

GENERIC_INIT(conf_section_t, conf_section);
static void conf_section_wipe(conf_section_t *section)
{
    p_delete(&section->name);
    while (section->var_nb > 0) {
        section->var_nb--;
        p_delete(&section->variables[section->var_nb]);
        p_delete(&section->values[section->var_nb]);
    }
    p_delete(&section->variables);
    p_delete(&section->values);
}

GENERIC_NEW(conf_section_t, conf_section);
GENERIC_DELETE(conf_section_t, conf_section);

void conf_wipe(conf_t *conf)
{
    while (conf->section_nb > 0) {
        conf->section_nb--;
        conf_section_delete(&conf->sections[conf->section_nb]);
    }
    p_delete(&conf->sections);
}


/**
 *  Parse functions
 *
 */

static void section_add_var(conf_section_t *section,
                            const char *variable, int variable_len,
                            const char *value, int value_len)
{
    int n = section->var_nb;

    section->variables = p_renew(char *, section->variables, n, n + 1);
    section->values    = p_renew(char *, section->values, n, n + 1);
    section->variables[n] = p_dupstr(variable, variable_len);
    section->values[n] = p_dupstr(value, value_len);
    section->var_nb++;
}

static void conf_add_section(conf_t *conf, conf_section_t *section)
{
    conf->sections = p_renew(conf_section_t *, conf->sections,
                             conf->section_nb, conf->section_nb + 1);
    conf->sections[conf->section_nb] = section;
    conf->section_nb++;
}

/* Read a line from the blob, return pointer to blob data or NULL at
 * EOF.
 */
static const char *readline(blob_t *buf, blob_t *buf_line)
{
    const char *line, *p;

    if (buf->len == 0) {
        blob_reset(buf_line);
        return NULL;
    }

    line = blob_get_cstr(buf);
    p = strchr(line, '\n');

    if (!p) {
        /* no final \n in file, treat that like a line */
        blob_set(buf_line, buf);
        blob_reset(buf);
    } else {
        blob_set_data(buf_line, line, p + 1 - line);
        blob_kill_first(buf, p + 1 - line);
    }
    return blob_get_cstr(buf_line);
}

/* Read a line from the blob, return pointer to blob data or NULL at
 * EOF.  Skip blank and comment lines.
 */
static const char *readline_skip(blob_t *buf, blob_t *buf_line)
{
    const char *line, *p;

    for (;;) {
        if ((line = readline(buf, buf_line)) == NULL)
            break;

        p = skipspaces(line);
        if (*p == '\0' || *p == '#' || *p == ';') {
            e_trace(CONF_DBG_LVL + 1, "Comment: %s", line);
            continue;
        } else {
            e_trace(CONF_DBG_LVL + 1, "Read line: %s", line);
            break;
        }
    }
    return line;
}


/* Returns conf_file or NULL if non-NULL file could not be opened */
/* Should try and load from a PATH of directories, and keep file name
 * in conf structure */
conf_t *conf_load(const char *filename)
{
    blob_t buf;
    conf_t *res;

    if (!filename) {
        /* NULL file: return empty conf */
        return conf_new();
    }

    blob_init(&buf);
    if (blob_append_file_data(&buf, filename) < 0) {
        e_trace(CONF_DBG_LVL, "could not open %s for reading (%m)", filename);
        blob_wipe(&buf);
        return NULL;
    }

    res = conf_load_blob(&buf);
    blob_wipe(&buf);

    return res;
}

/* Same as conf_load, but from a blob. XXX: buf may be modified */
conf_t *conf_load_blob(blob_t *buf)
{
    conf_t *conf;
    blob_t buf_line;
    const char *line, *start, *stop;
    const char *variable, *value;
    int len, variable_len, value_len;
    conf_section_t *section;

    /* OG: Should parse without the need for a line blob */
    blob_init(&buf_line);

    conf = conf_new();
    section = NULL;

    for (;;) {

        /* Read a new line */
        line = readline_skip(buf, &buf_line);

        if (!line) {
            e_trace(CONF_DBG_LVL + 1, "End of input...");
            break;
        }

        /* Skip first spaces */
        start = skipspaces(line);

        /* Check start of section name */
        if (*start == '[') {
            start += 1;   /* skip '[' */
            /* Search end of section name */
            stop = strchr(start, ']');
            if (!stop) {
                e_trace(CONF_DBG_LVL, "Invalid section header: %s", line);
                continue;
            }

            /* Copy name in the section struct */
            section = conf_section_new();
            section->name = p_dupstr(start, stop - start);

            e_trace(CONF_DBG_LVL + 1, "section name: %s",
                    section->name);

            /* Add this section in the conf struct */
            conf_add_section(conf, section);

            e_trace(CONF_DBG_LVL + 1,
                    " Read 1 section name: buffer remaining:%s", stop + 1);
            continue;
        }

        if (!section) {
            e_trace(CONF_DBG_LVL, "Junk line: %s", line);
            continue;
        }

        /* Check for variable definition in section */

        /* Search end of var */
        stop = strchr(start, '=');
        if (!stop) {
            /* No value */
            e_trace(CONF_DBG_LVL, "No value on this line -> dropped: %s",
                    line);
            continue;
        }

        /* Trim variable name */
        len = stop - start;
        while (len > 0 && isspace((unsigned char)start[len - 1])) {
            len -= 1;
        }
        variable = start;
        variable_len = len;

        e_trace(CONF_DBG_LVL + 1, "  varname: %.*s", variable_len, variable);

        /* Skip spaces after = */
        start = skipspaces(stop + 1);

        /* Read until end of line */
        stop = strchr(start, '\n');
        if (!stop) {
            /* EOF */
            stop = strchr(start, '\0');
        }

        /* Trim value */
        len = stop - start;
        while (len > 0 && isspace((unsigned char)start[len - 1])) {
            len -= 1;
        }

        value = start;
        value_len = len;

        e_trace(CONF_DBG_LVL + 1, "  value  : %.*s", value_len, value);

        /* OG: no support for escaping */
        section_add_var(section, variable, variable_len, value, value_len);
    }

    blob_wipe(&buf_line);

    return conf;
}

int conf_save(const conf_t *conf, const char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL) {
        e_trace(CONF_DBG_LVL, "could not open %s for writing", filename);
        return -1;
    }

    if (conf) {
        int i, j;
        conf_section_t *section;

        for (i = 0; i < conf->section_nb; i++) {
            section = conf->sections[i];

            fprintf(fp, "[%s]\n", section->name);
            for (j = 0; j < section->var_nb; j++) {
                fprintf(fp, "%s = %s\n",
                        section->variables[j], section->values[j]);
            }
            fprintf(fp, "\n");
        }
    }
    fclose(fp);
    return 0;
}

const char *conf_get_raw(const conf_t *conf, const char *section, const char *var)
{
    int i, j;
    conf_section_t *s;

    assert (section && var);

    for (i = 0; i < conf->section_nb; i++) {
        s = conf->sections[i];
        if (!strcasecmp(s->name, section)) {
            for (j = 0; j < s->var_nb; j++) {
                if (!strcasecmp(s->variables[j], var)) {
                    return s->values[j];
                }
            }
            return NULL;
        }
    }
    return NULL;
}


int conf_get_int(const conf_t *conf, const char *section,
                 const char *var, int defval)
{
    const char *val = conf_get_raw(conf, section, var);
    int res;

    if (!val)
        return defval;

    res = strtoip(val, &val);
    /* OG: this test is too strong: if the value of the setting is not
     * exactly a number, we should have a more specific way of telling
     * the caller about it.  Just returning the default value may not
     * be the best option.
     */
    return *val ? defval : res;
}

int conf_get_bool(const conf_t *conf, const char *section,
                  const char *var, int defval)
{
    const char *val = conf_get_raw(conf, section, var);
    if (!val)
        return defval;

#define CONF_CHECK_BOOL(name, value) \
    if (!strcasecmp(val, name)) {    \
        return value;                \
    }
    CONF_CHECK_BOOL("true",  true);
    CONF_CHECK_BOOL("false", false);
    CONF_CHECK_BOOL("on",    true);
    CONF_CHECK_BOOL("off",   false);
    CONF_CHECK_BOOL("yes",   true);
    CONF_CHECK_BOOL("no",    false);
    CONF_CHECK_BOOL("1",     true);
    CONF_CHECK_BOOL("0",     false);
#undef CONF_CHECK_BOOL

    return defval;
}

const char *conf_put(conf_t *conf, const char *section,
                     const char *var, const char *value)
{
    int i;
    conf_section_t *s;
    int value_len = value ? strlen(value) : 0;
    int var_len = var ? strlen(var) : 0;

    if (!section || !var) {
        return NULL;
    }

    for (i = 0; i < conf->section_nb; i++) {
        s = conf->sections[i];
        if (!strcasecmp(s->name, section)) {
            int j;

            for (j = 0; j < s->var_nb; j++) {
                if (strcasecmp(s->variables[j], var))
                    continue;
                if (value) {
                    if (!strcmp(s->values[j], value)) {
                        /* same value already: no nothing */
                        return s->values[j];
                    }
                    /* replace value */
                    p_delete(&s->values[j]);
                    return s->values[j] = p_dupstr(value, value_len);
                } else {
                    /* delete value */
                    p_delete(&s->variables[j]);
                    p_delete(&s->values[j]);
                    s->var_nb--;
                    memmove(&s->variables[j], &s->variables[j + 1],
                            sizeof(s->variables[j]) * (s->var_nb - j));
                    memmove(&s->values[j], &s->values[j + 1],
                            sizeof(s->values[j]) * (s->var_nb - j));
                    return NULL;
                }
            }
            if (value) {
                /* add variable in existing section */
                section_add_var(s, var, var_len, value, value_len);
                return s->values[j];
            }
        }
    }
    if (value) {
        /* add variable in new section */
        s = conf_section_new();
        s->name = p_strdup(section);
        conf_add_section(conf, s);
        section_add_var(s, var, var_len, value, value_len);
        return s->values[0];
    }
    return NULL;
}

int
conf_next_section_idx(const conf_t *conf, const char *prefix,
                      int prev_idx, const char **suffix)
{
    int i;

    if (prev_idx < 0) {
        prev_idx = 0;
    } else
    if (prev_idx >= conf->section_nb - 1) {
        return -1;
    } else {
        prev_idx += 1;
    }

    for (i = prev_idx; i < conf->section_nb; i++) {
        if (stristart(conf->sections[i]->name, prefix, suffix)) {
            return i;
        }
    }

    return -1;
}


#ifdef CHECK /* {{{ */

START_TEST(check_conf_load)
{
#define SAMPLE_CONF_FILE "samples/example.conf"
#define SAMPLE_SECTION1_NAME  "section1"
#define SAMPLE_SECTION_NB  3
#define SAMPLE_SECTION1_VAR_NB  2
#define SAMPLE_SECTION1_VAR1_NAME  "param1"
#define SAMPLE_SECTION1_VAR2_NAME  "param2[]@!!sdf"
#define SAMPLE_SECTION1_VAL1 "123 456"

    conf_t *conf;
    conf_section_t *s;
    blob_t blob;
    int prev;
    const char *p;

    conf = conf_load(SAMPLE_CONF_FILE);
    fail_if(conf == NULL,
            "conf_load failed");
    fail_if(conf->section_nb != SAMPLE_SECTION_NB,
            "conf_load did not parse the right number of sections (%d != %d)",
            conf->section_nb, SAMPLE_SECTION_NB);

    s = conf->sections[0];
    fail_if(!strequal(s->name, SAMPLE_SECTION1_NAME),
            "bad section name: expected '%s', got '%s'",
            SAMPLE_SECTION1_NAME, s->name);
    fail_if(s->var_nb != SAMPLE_SECTION1_VAR_NB,
            "bad variable number for section '%s': expected %d, got %d",
            s->name, SAMPLE_SECTION1_VAR_NB, s->var_nb);
    fail_if(!strequal(s->variables[0], SAMPLE_SECTION1_VAR1_NAME),
            "bad variable name: expected '%s', got '%s'",
            SAMPLE_SECTION1_VAR1_NAME, s->variables[0]);
    fail_if(!strequal(s->variables[1], SAMPLE_SECTION1_VAR2_NAME),
            "bad variable name: expected '%s', got '%s'",
            SAMPLE_SECTION1_VAR2_NAME, s->variables[1]);
    fail_if(!strequal(s->values[0], SAMPLE_SECTION1_VAL1),
            "bad variable value: expected '%s', got '%s'",
            SAMPLE_SECTION1_VAL1, s->values[0]);

    prev = -1;
    prev = conf_next_section_idx(conf, "section", prev, NULL);
    fail_if(prev != 0,
            "bad next section idx: expected %d, got %d",
            0, prev);
    prev = conf_next_section_idx(conf, "section", prev, NULL);
    fail_if(prev != 1,
            "bad next section idx: expected %d, got %d",
            1, prev);
    prev = conf_next_section_idx(conf, "section", prev, &p);
    fail_if(prev != 2,
            "bad next section idx: expected %d, got %d",
            2, prev);
    fail_if(!strequal(p, "3"),
            "bad next section suffix: expected '%s', got '%s'",
            "3", p);

    prev = -1;
    prev = conf_next_section_idx(conf, "section1", prev, NULL);
    fail_if(prev != 0,
            "bad next section idx: expected %d, got %d",
            0, prev);
    prev = conf_next_section_idx(conf, "section1", prev, &p);
    fail_if(prev != 1,
            "bad next section idx: expected %d, got %d",
            1, prev);
    fail_if(!strequal(p, "2"),
            "bad next section suffix: expected '%s', got '%s'",
            "2", p);
    prev = conf_next_section_idx(conf, "section1", prev, &p);
    fail_if(prev != -1,
            "bad next section idx: expected %d, got %d",
            -1, prev);


    conf_delete(&conf);

    blob_init(&blob);
    fail_if(blob_append_file_data(&blob, SAMPLE_CONF_FILE) < 0,
            "Could not read sample file: %s", SAMPLE_CONF_FILE);

    conf = conf_load_blob(&blob);
    
    fail_if(conf == NULL,
            "conf_load_blob failed");
    fail_if(conf->section_nb != SAMPLE_SECTION_NB,
            "conf_load_blob did not parse the right number of sections (%d != %d)",
            conf->section_nb, SAMPLE_SECTION_NB);
    conf_delete(&conf);
    blob_wipe(&blob);

#undef SAMPLE_CONF_FILE
}
END_TEST

Suite *check_conf_suite(void)
{
    Suite *s  = suite_create("Conf");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_conf_load);

    return s;
}

#endif /* }}} */
