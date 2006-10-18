/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
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

#include "conf.h"

#define CONF_DBG_LVL 3

static conf_section_t *conf_section_init(conf_section_t *section)
{
    p_clear(section, 1);
    return section;
}
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

static conf_t *conf_init(conf_t *conf)
{
    conf->sections = NULL;
    conf->section_nb = 0;
    return conf;
}
GENERIC_NEW(conf_t, conf);
void conf_wipe(conf_t *conf)
{
    while (conf->section_nb > 0) {
        conf->section_nb--;
        conf_section_delete(&conf->sections[conf->section_nb]);
    }
    p_delete(&conf->sections);
}

const char *conf_get(const conf_t *conf, const char *section, const char *var)
{
    int i;
    conf_section_t *s;

    if (!section || !var) {
        return NULL;
    }

    for (i = 0; i < conf->section_nb; i++) {
        s = conf->sections[i];
        if (!strcasecmp(s->name, section)) {
            for (i = 0; i < s->var_nb; i++) {
                if (!strcasecmp(s->variables[i], var)) {
                    return s->values[i];
                }
            }
            return NULL;
        }
    }
    return NULL;
}


/**
 *  Parse functions
 *
 */

static void section_add_var(conf_section_t *section,
                            const char *var, const char *var_end,
                            const char *val, const char *val_end)
{
    int i;

    section->variables = p_renew(char *, section->variables,
                                 section->var_nb,
                                 section->var_nb + 1);
    section->values    = p_renew(char *, section->values,
                                 section->var_nb,
                                 section->var_nb + 1);
    i = var_end - var;
    section->variables[section->var_nb] = p_new(char, i + 1);
    pstrcpylen(section->variables[section->var_nb], i + 1, var, i);
    strrtrim(section->variables[section->var_nb]);
    i = val_end - val;
    section->values[section->var_nb] = p_new(char, i + 1);
    pstrcpylen(section->values[section->var_nb], i + 1, val, i);
    strrtrim(section->values[section->var_nb]);
    section->var_nb++;
}

static void conf_add_section(conf_t *conf, conf_section_t *section)
{
    conf->sections = p_renew(conf_section_t *, conf->sections,
                             conf->section_nb, conf->section_nb + 1);
    conf->sections[conf->section_nb] = section;
    conf->section_nb++;
}

/* read a non empty line, return error if no more such lines */
static int readline_aux(blob_t *buf, blob_t *line)
{
    const char *begin;
    const char *p = blob_get_cstr(buf);

    /* skip empty lines, XXX: isspace(\0) is false */
    while (isspace(*p)) {
        p++;
    }
    blob_kill_first(buf, p - blob_get_cstr(buf));

    if (buf->len == 0) {
        blob_resize(line, 0);
        return -1;
    }

    begin = blob_get_cstr(buf);
    p = strchr(begin, '\n');

    if (!p) {
        /* no final \n in file, treat that like a line */
        blob_set(line, buf);
        blob_resize(buf, 0);
    } else {
        blob_set_data(line, buf->data, p - begin);
        blob_kill_first(buf, p + 1 - begin);
    }
    return 0;
}

static void readline(blob_t *buf, blob_t *line)
{
    for (;;) {
        if (readline_aux(buf, line))
            return;

        if (line->data[0] == '#' || line->data[0] == ';') {
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("Comment : %s\n"),
                    blob_get_cstr(line));
        } else {
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("Read line : %s\n"),
                    blob_get_cstr(line));
            return;
        }
    }
}

/**
 *  Return values
 *
 *  1 : could not open filename
 *  2 : empty file
 *
 */
int parse_ini(const char *filename, conf_t **conf)
{
    blob_t buf;
    blob_t buf_line;
    int len;
    const char *p, *start;
    const char *var_start, *var_end;
    const char *val_start, *val_end;
    conf_section_t *section;

    blob_init(&buf);
    blob_init(&buf_line);

    if (blob_append_file_data(&buf, filename) < 0) {
        e_debug(CONF_DBG_LVL, E_PREFIX("could not open %s for reading\n"),
                filename);
    }

    if (buf.len == 0) {
        return 2;
    }

    *conf = conf_new();

    while (buf.len > 0) {

        /* Read a new line */
        readline(&buf, &buf_line);

        if (buf_line.len == 0) {
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("End of input...\n"));
            break;
        }

     section_parse:
        /* Search start of section name */
        p = strchr(blob_get_cstr(&buf_line), '[');
        if (!p) {
            e_debug(CONF_DBG_LVL, E_PREFIX("Junk line : %s\n"),
                    blob_get_cstr(&buf_line));
            continue;
        }
        blob_kill_first(&buf_line, p - blob_get_cstr(&buf_line) + 1);

        start = blob_get_cstr(&buf_line);

        /* Search end of section name */
        p = strchr(start, ']');
        if (!p) {
            e_debug(CONF_DBG_LVL,
                    E_PREFIX("Could not find section name end : %s\n"),
                    blob_get_cstr(&buf_line));
            continue;
        }

        len = p - start;
        /* Copy name in the section struct */
        section = conf_section_new();
        section->name = p_new(char, len + 1);
        pstrcpylen(section->name, len + 1, start, len);

        e_debug(CONF_DBG_LVL + 1, E_PREFIX("section name : %s\n"),
                section->name);

        /* Add this section in the conf struct */
        conf_add_section(*conf, section);

        e_debug(CONF_DBG_LVL + 1,
                E_PREFIX(" Read 1 section name : buffer remaining:\n%s"),
                blob_get_cstr(&buf));

        /* Add varnames in section */
        while (buf.len > 0) {
            readline(&buf, &buf_line);

            if (buf_line.len == 0) {
                break;
            }

            p = blob_get_cstr(&buf_line);
            var_start = skipspaces(p);
            /* Skip first spaces */
            blob_kill_first(&buf_line, var_start - p);

            var_start = blob_get_cstr(&buf_line);
            /* Check that we are not reading a section name instead */
            if (*var_start == '[') {
                /* This line is a section name */
                goto section_parse;
            }
            /* Search end of var */
            var_end = strchr(var_start, '=');
            if (!var_end) {
                /* No value */
                e_debug(CONF_DBG_LVL,
                        E_PREFIX("No value on this line -> dropped : %s\n"),
                        blob_get_cstr(&buf_line));
                continue;
                continue;
            }
            len = var_end - var_start;
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("  varname : %.*s\n"),
                    len, var_start);
            p = var_end + 1;
            /* Skip spaces after = */
            val_start = skipspaces(p);

            /* Read until end of line */
            p = strchr(val_start, '\n');
            if (p) {
                val_end = p;
                e_debug(CONF_DBG_LVL + 1,
                        E_PREFIX(" next '\\n' at %d\n"),
                        (int)(p - var_start));
            } else {
                /* EOF */
                val_end = var_start + buf_line.len;
            }
            e_debug(CONF_DBG_LVL + 1, E_PREFIX("  value   : %.*s\n"),
                    (int)(val_end - val_start), val_start);
            section_add_var(section, var_start, var_end, val_start, val_end);
        }
    }

    blob_wipe(&buf);
    blob_wipe(&buf_line);

    return 0;
}

void conf_dump(int level, const conf_t *conf)
{
    int i, j;
    conf_section_t *section;

    e_debug(level, "Conf with %d sections :\n", conf->section_nb);

    for (i = 0; i < conf->section_nb; i++) {
        section = conf->sections[i];

        e_debug(level, "[%s]\n", section->name);
        for (j = 0; j < section->var_nb; j++) {
            e_debug(level, "%s = %s\n", section->variables[j],
                    section->values[j]);
        }
    }
}

