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

#include "conf.h"

#include <dirent.h>

static conf_section_t *conf_section_init(conf_section_t *section)
{
    props_array_init(&section->vals);
    return section;
}
static void conf_section_wipe(conf_section_t *section)
{
    p_delete(&section->name);
    props_array_wipe(&section->vals);
}
GENERIC_NEW(conf_section_t, conf_section);
GENERIC_DELETE(conf_section_t, conf_section);
ARRAY_FUNCTIONS(conf_section_t, conf_section, conf_section_delete);
void conf_delete(conf_t **conf)
{
    conf_section_array_delete(conf);
}

static int conf_parse_hook(void *_conf, cfg_parse_evt evt,
                           const char *v, int vlen, void *ctx)
{
    conf_t *conf = _conf;

    switch (evt) {
        conf_section_t *sect;
        property_t *prop;

      case CFG_PARSE_SECTION:
        sect = conf_section_new();
        sect->name = p_dupz(v, vlen);
        conf_section_array_append(conf, sect);
        return 0;

      case CFG_PARSE_SECTION_ID:
        assert (v == NULL);
        return 0;

      case CFG_PARSE_KEY:
        sect = conf->tab[conf->len - 1];
        prop = property_new();
        prop->name = p_dupz(v, vlen);
        props_array_append(&sect->vals, prop);
        return 0;

      case CFG_PARSE_VALUE:
        sect = conf->tab[conf->len - 1];
        prop = sect->vals.tab[sect->vals.len - 1];
        prop->value = v ? p_dupz(v, vlen) : NULL;
        return 0;

      case CFG_PARSE_SET:
      case CFG_PARSE_EOF:
        return 0;

      case CFG_PARSE_ERROR:
        e_error("%s", v);
        return 0;

      case CFG_PARSE_KEY_ARRAY:
      case CFG_PARSE_ARRAY_OPEN:
      case CFG_PARSE_APPEND:
      case CFG_PARSE_ARRAY_CLOSE:
        e_panic("should not happen");
    }
    return -1;
}

#define CONF_PARSE_OPTS  (CFG_PARSE_OLD_NAMESPACES | CFG_PARSE_OLD_KEYS)
conf_t *conf_load(const char *filename)
{
    conf_t *res = conf_section_array_new();
    if (cfg_parse(filename, &conf_parse_hook, res, CONF_PARSE_OPTS))
        conf_delete(&res);
    return res;
}

int conf_merge_dir(conf_t *conf, const char *path)
{
    struct dirent *file;
    DIR *conf_dir = opendir(path);

    if (!conf_dir) {
        return e_error("cannot open configuration directory <%s>", path);
    }

    while ((file = readdir(conf_dir))) {
        char buf[PATH_MAX];
        pstream_t file_ps = ps_initstr(file->d_name);

        if (file_ps.s[0] == '.')
            continue;

        if (ps_skip_upto(&file_ps, file_ps.b_end - strlen(".ini")))
            continue;

        if (ps_strequal(&file_ps, ".ini")) {
            snprintf(buf, sizeof(buf), "%s/%s", path, file->d_name);
            if (cfg_parse(buf, &conf_parse_hook, conf, CONF_PARSE_OPTS)) {
                closedir(conf_dir);
                return e_error("cannot parse <%s>", buf);
            }
        }
    }

    closedir(conf_dir);
    return 0;
}

conf_t *conf_load_str(const char *s, int len)
{
    conf_t *res = conf_section_array_new();
    if (cfg_parse_buf(s, len, &conf_parse_hook, res, CONF_PARSE_OPTS))
        conf_delete(&res);
    return res;
}

static void section_add_var(conf_section_t *section,
                            const char *variable, int variable_len,
                            const char *value, int value_len)
{
    property_t *prop = property_new();
    prop->name  = p_dupz(variable, variable_len);
    prop->value = p_dupz(value, value_len);
    props_array_append(&section->vals, prop);
}

int conf_save(const conf_t *conf, const char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL) {
        e_trace(1, "could not open %s for writing", filename);
        return -1;
    }

    if (conf) {
        int i, j;
        conf_section_t *section;

        for (i = 0; i < conf->len; i++) {
            section = conf->tab[i];

            fprintf(fp, "[%s]\n", section->name);
            for (j = 0; j < section->vals.len; j++) {
                fprintf(fp, "%s = %s\n",
                        section->vals.tab[j]->name,
                        section->vals.tab[j]->value);
            }
            fprintf(fp, "\n");
        }
    }
    fclose(fp);
    return 0;
}

const char *conf_section_get_raw(const conf_section_t *section,
                                 const char *var)
{
    int i;

    assert (section && var);

    for (i = 0; i < section->vals.len; i++) {
        if (!strcasecmp(section->vals.tab[i]->name, var)) {
            return section->vals.tab[i]->value;
        }
    }
    return NULL;
}

const char *conf_get_raw(const conf_t *conf, const char *section,
                         const char *var)
{
    int i;
    conf_section_t *s;

    assert (section && var);

    for (i = 0; i < conf->len; i++) {
        s = conf->tab[i];
        if (!strcasecmp(s->name, section)) {
            return conf_section_get_raw(s, var);
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

int conf_get_verbosity(const conf_t *conf, const char *section,
                       const char *var, int defval)
{
    const char *val = conf_get_raw(conf, section, var);
    int res;

    if (!val)
        return defval;

    res = atoi(val);
    if (res > 1 && res < 8) {
        return res;
    } else {
        if (strequal(val, "PANIC")) {
            return 2;
        }
        if (strequal(val, "ERROR")) {
            return 3;
        }
        if (strequal(val, "WARNING")) {
            return 4;
        }
        if (strequal(val, "NORMAL")) {
            return 5;
        }
        if (strequal(val, "INFO")) {
            return 6;
        }
        if (strequal(val, "DEBUG")) {
            return 7;
        }
    }
    return defval;
}

int conf_section_get_int(const conf_section_t *section,
                         const char *var, int defval)
{
    const char *val = conf_section_get_raw(section, var);
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

int conf_section_get_bool(const conf_section_t *section,
                          const char *var, int defval)
{
    const char *val = conf_section_get_raw(section, var);
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

    for (i = 0; i < conf->len; i++) {
        s = conf->tab[i];
        if (!strcasecmp(s->name, section)) {
            int j;

            for (j = 0; j < s->vals.len; j++) {
                property_t *prop = s->vals.tab[j];
                if (strcasecmp(prop->name, var))
                    continue;
                if (value) {
                    if (!strcmp(prop->value, value)) {
                        /* same value already: no nothing */
                        return prop->value;
                    }
                    /* replace value */
                    p_delete(&prop->value);
                    return prop->value = p_dupz(value, value_len);
                } else {
                    /* delete value */
                    props_array_remove(&s->vals, j);
                    property_delete(&prop);
                    return NULL;
                }
            }
            if (value) {
                /* add variable in existing section */
                section_add_var(s, var, var_len, value, value_len);
                return s->vals.tab[j]->value;
            }
        }
    }
    if (value) {
        /* add variable in new section */
        s = conf_section_new();
        s->name = p_strdup(section);
        conf_section_array_append(conf, s);
        section_add_var(s, var, var_len, value, value_len);
        return s->vals.tab[0]->value;
    }
    return NULL;
}

const conf_section_t *
conf_get_section_by_name(const conf_t *conf, const char *name)
{
    int i;

    for (i = 0; i < conf->len; i++) {
        if (strequal(name, conf->tab[i]->name)) {
            return conf->tab[i];
        }
    }
    return NULL;
}


int conf_next_section_idx(const conf_t *conf, const char *prefix,
                          int prev_idx, const char **suffixp)
{
    int i;

    if (prev_idx < 0) {
        prev_idx = 0;
    } else
    if (prev_idx >= conf->len - 1) {
        return -1;
    } else {
        prev_idx += 1;
    }

    for (i = prev_idx; i < conf->len; i++) {
        if (stristart(conf->tab[i]->name, prefix, suffixp)) {
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
#define SAMPLE_SECTION1_VAR2_NAME  "param2[]sdf"
#define SAMPLE_SECTION1_VAL1 "123 456"

    conf_t *conf;
    conf_section_t *s;
    sb_t sb;
    int prev;
    int verb;
    const char *p;

    conf = conf_load(SAMPLE_CONF_FILE);
    fail_if(conf == NULL,
            "conf_load failed");
    fail_if(conf->len != SAMPLE_SECTION_NB,
            "conf_load did not parse the right number of sections (%d != %d)",
            conf->len, SAMPLE_SECTION_NB);

    s = conf->tab[0];
    fail_if(!strequal(s->name, SAMPLE_SECTION1_NAME),
            "bad section name: expected '%s', got '%s'",
            SAMPLE_SECTION1_NAME, s->name);
    fail_if(s->vals.len != SAMPLE_SECTION1_VAR_NB,
            "bad variable number for section '%s': expected %d, got %d",
            s->name, SAMPLE_SECTION1_VAR_NB, s->vals.len);
    fail_if(!strequal(s->vals.tab[0]->name, SAMPLE_SECTION1_VAR1_NAME),
            "bad variable name: expected '%s', got '%s'",
            SAMPLE_SECTION1_VAR1_NAME, s->vals.tab[0]->name);
    fail_if(!strequal(s->vals.tab[1]->name, SAMPLE_SECTION1_VAR2_NAME),
            "bad variable name: expected '%s', got '%s'",
            SAMPLE_SECTION1_VAR2_NAME, s->vals.tab[1]->name);
    fail_if(!strequal(s->vals.tab[0]->value, SAMPLE_SECTION1_VAL1),
            "bad variable value: expected '%s', got '%s'",
            SAMPLE_SECTION1_VAL1, s->vals.tab[0]->value);

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

    /* Check on conf_get_verbosity */
    verb = conf_get_verbosity(conf, "section3", "log_verbosity1", 10);
    fail_if(verb != 10,
            "bad get verbosity: expected %d, got %d",
            10, verb);
    verb = conf_get_verbosity(conf, "section3", "log_verbosity2", 10);
    fail_if(verb != 2,
            "bad get verbosity: expected %d, got %d",
            2, verb);
    verb = conf_get_verbosity(conf, "section3", "log_verbosity3", 10);
    fail_if(verb != 2,
            "bad get verbosity: expected %d, got %d",
            2, verb);
    verb = conf_get_verbosity(conf, "section3", "log_verbosity4", 10);
    fail_if(verb != 6,
            "bad get verbosity: expected %d, got %d",
            6, verb);
    verb = conf_get_verbosity(conf, "section3", "log_verbosity5", 10);
    fail_if(verb != 10,
            "bad get verbosity: expected %d, got %d",
            10, verb);
    verb = conf_get_verbosity(conf, "section3", "log_verbosity6", 10);
    fail_if(verb != 10,
            "bad get verbosity: expected %d, got %d",
            10, verb);

    conf_delete(&conf);

    sb_init(&sb);
    fail_if(sb_read_file(&sb, SAMPLE_CONF_FILE) < 0,
            "Could not read sample file: %s", SAMPLE_CONF_FILE);

    conf = conf_load_str(sb.data, sb.len);

    fail_if(conf == NULL,
            "conf_load_str failed");
    fail_if(conf->len != SAMPLE_SECTION_NB,
            "conf_load_str did not parse the right number of sections (%d != %d)",
            conf->len, SAMPLE_SECTION_NB);
    conf_delete(&conf);
    sb_wipe(&sb);

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
